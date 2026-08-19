"""Minimal RFC 6455 helpers shared by physical and mock camera HIL tests."""

from __future__ import annotations

import socket


def read_exact(connection: socket.socket, size: int) -> bytes:
    """Reads exactly one WebSocket field and rejects premature disconnects."""

    result = bytearray()
    while len(result) < size:
        block = connection.recv(size - len(result))
        if not block:
            raise AssertionError("WebSocket closed before a complete frame arrived")
        result.extend(block)
    return bytes(result)


def receive_frame(connection: socket.socket) -> tuple[int, bytes]:
    """Decodes one unmasked server-to-host WebSocket frame."""

    first, second = read_exact(connection, 2)
    opcode = first & 0x0F
    if second & 0x80:
        raise AssertionError("server WebSocket frame must not be masked")
    length = second & 0x7F
    if length == 126:
        length = int.from_bytes(read_exact(connection, 2), "big")
    elif length == 127:
        length = int.from_bytes(read_exact(connection, 8), "big")
    return opcode, read_exact(connection, length)


def send_text(connection: socket.socket, payload: bytes) -> None:
    """Sends one masked, compact client text frame."""

    if len(payload) >= 126:
        raise ValueError("compact HIL text frame payload is too large")
    mask = b"\x12\x34\x56\x78"
    encoded = bytes(value ^ mask[index % 4] for index, value in enumerate(payload))
    connection.sendall(bytes((0x81, 0x80 | len(payload))) + mask + encoded)


def open_video_socket(host: str, path: str = "/ws_video") -> socket.socket:
    """Completes a bounded WebSocket upgrade against the video server."""

    connection = socket.create_connection((host, 82), timeout=5.0)
    connection.settimeout(8.0)
    request = (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: {host}:82\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n"
    ).encode("ascii")
    connection.sendall(request)
    response = bytearray()
    while b"\r\n\r\n" not in response:
        block = connection.recv(1024)
        if not block:
            connection.close()
            raise AssertionError("video server closed during WebSocket upgrade")
        response.extend(block)
    if not response.startswith(b"HTTP/1.1 101"):
        connection.close()
        raise AssertionError(f"video WebSocket upgrade failed: {bytes(response)!r}")
    return connection
