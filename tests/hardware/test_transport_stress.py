"""Recoverable USB and TCP framing, concurrency, and capacity checks."""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import socket
import time

import pytest

from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    TcpProtocolClient,
    receive_tcp_frames,
)
from tools.wifi_provision_protocol import encode_frame


def _wait_for_tcp_slot_release(host: str, timeout_seconds: float = 8.0) -> None:
    """Waits until one ordinary request proves that a client slot is reusable."""
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        try:
            if TcpProtocolClient(host).exchange(GENERAL_COMMAND, b"ftype /", 1.0):
                time.sleep(0.25)
                return
        except (ConnectionResetError, OSError):
            pass
        time.sleep(0.1)
    pytest.fail("TCP client slots were not released")


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.usb
@pytest.mark.requirement("USB-007")
def test_usb_repeated_requests_keep_transport_responsive(usb_client) -> None:
    for _ in range(20):
        frames = usb_client.exchange(GENERAL_COMMAND, b"ftype /", 2.0)
        assert frames


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.usb
@pytest.mark.requirement("USB-009")
@pytest.mark.requirement("FRM-016")
def test_usb_recovers_after_unframed_noise(usb_client) -> None:
    usb_client.output.write(b"not-a-frame", timeout=usb_client.timeout_ms)
    frames = usb_client.exchange(GENERAL_COMMAND, b"ftype /", 3.0)
    assert frames


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.tcp
@pytest.mark.requirement("TCP-003")
@pytest.mark.requirement("FRM-010")
def test_tcp_accepts_one_frame_split_across_writes(tcp_host: str) -> None:
    encoded = encode_frame(GENERAL_COMMAND, b"ftype /")
    with socket.create_connection((tcp_host, 2222), timeout=3.0) as connection:
        for byte in encoded:
            connection.sendall(bytes([byte]))
            time.sleep(0.002)
        assert receive_tcp_frames(connection, 3.0)


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.tcp
@pytest.mark.requirement("TCP-001")
@pytest.mark.requirement("TCP-010")
def test_tcp_fifth_connection_receives_capacity_response(tcp_host: str) -> None:
    _wait_for_tcp_slot_release(tcp_host)
    connections: list[socket.socket] = []
    rejected_frames = []
    accepted_count = 0
    try:
        for _ in range(4):
            connection = socket.create_connection((tcp_host, 2222), timeout=3.0)
            connection.sendall(encode_frame(GENERAL_COMMAND, b"ftype /"))
            try:
                frames = receive_tcp_frames(connection, 3.0)
            except ConnectionResetError:
                break
            if not frames:
                break
            connections.append(connection)
            accepted_count += 1
        if accepted_count == 4:
            with socket.create_connection((tcp_host, 2222), timeout=3.0) as rejected:
                rejected_frames = receive_tcp_frames(rejected, 3.0)
    finally:
        for connection in connections:
            connection.close()
        _wait_for_tcp_slot_release(tcp_host)
    assert accepted_count == 4, (
        f"target accepted only {accepted_count} simultaneous TCP clients"
    )
    assert any(frame.frame_type == 0x91 for frame in rejected_frames)


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.tcp
@pytest.mark.requirement("TCP-003")
@pytest.mark.requirement("TCP-006")
def test_tcp_concurrent_clients_receive_independent_responses(tcp_host: str) -> None:
    _wait_for_tcp_slot_release(tcp_host)
    def request() -> bool:
        try:
            return bool(TcpProtocolClient(tcp_host).exchange(
                GENERAL_COMMAND, b"ftype /", 4.0
            ))
        except (ConnectionResetError, OSError):
            return False

    with ThreadPoolExecutor(max_workers=4) as executor:
        assert all(executor.map(lambda _: request(), range(4)))
