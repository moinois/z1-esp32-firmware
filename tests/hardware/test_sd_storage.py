"""Physical SD and filesystem checks routed through the public host protocol."""

from __future__ import annotations

import hashlib
import os
import socket
import time
import uuid

import pytest

from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    TcpProtocolClient,
    receive_tcp_frames,
)
from tests.hardware.hil_file_transfer import (
    FILE_COMMAND,
    FILE_COMPLETE,
    FILE_CANCEL,
    FILE_DATA,
    FILE_GEOMETRY,
    FILE_MD5,
    FileTransferError,
    download_file,
    upload_file,
)
from tools.wifi_provision_protocol import encode_frame


def _payload(frames) -> bytes:
    """Combines textual target responses for concise command assertions."""

    return b"\n".join(frame.payload for frame in frames)


def _remove(client, path: str) -> None:
    """Removes one test path as best-effort cleanup."""

    try:
        client.exchange(GENERAL_COMMAND, f"rm -R {path}".encode("ascii"), 5.0)
    except Exception:
        # Preserve the original assertion or transport failure if the target
        # disconnected before cleanup could run.
        pass


def _tcp_transfer_exchange(
    connection: socket.socket, frame_type: int, payload: bytes
):
    """Sends one transfer frame on an intentionally persistent TCP socket."""

    connection.sendall(encode_frame(frame_type, payload))
    # Wait beyond HFT-022's 5.010-second retry boundary. A slow or briefly
    # unavailable Wi-Fi link must not be declared failed before the firmware
    # has had a chance to request continuation.
    return receive_tcp_frames(connection, 7.0)


def _required_frame(frames, frame_type: int):
    """Returns one required response and exposes target errors in assertions."""

    errors = [frame.payload for frame in frames if frame.frame_type == FILE_CANCEL]
    assert not errors, errors[0].decode("utf-8", errors="replace")
    return next(
        (frame for frame in frames if frame.frame_type == frame_type),
        None,
    )


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.sd
@pytest.mark.requirement("SD-001")
@pytest.mark.requirement("FILE-005")
def test_sd_root_can_be_listed(sd_client, sd_fixture) -> None:
    frames = sd_client.exchange(GENERAL_COMMAND, b"ls /", timeout_seconds=5.0)
    assert frames, "no response; SD card may be absent or unmounted"
    combined = b"\n".join(frame.payload for frame in frames).lower()
    assert b"error" not in combined, combined.decode("utf-8", errors="replace")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.requirement("FILE-020")
@pytest.mark.requirement("FILE-021")
@pytest.mark.requirement("FILE-005")
def test_temporary_directory_create_and_remove(sd_client, sd_fixture) -> None:
    """Creates and removes a unique directory after explicit mutation opt-in."""
    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    # The product uses FAT short-name mode; keep the unique directory component
    # within the eight-character 8.3 base-name limit.
    path = f"/Z1{uuid.uuid4().hex[:6].upper()}"
    try:
        created = sd_client.exchange(
            GENERAL_COMMAND, f"mkdir {path}".encode("ascii"), timeout_seconds=5.0
        )
        assert created
        payload = b"\n".join(frame.payload for frame in created).lower()
        assert b"created directory" in payload, payload.decode(errors="replace")
        assert b"created directory /" in payload, payload.decode(errors="replace")
    finally:
        removed = sd_client.exchange(
            GENERAL_COMMAND, f"rm -R {path}".encode("ascii"), timeout_seconds=5.0
        )
    assert removed
    payload = b"\n".join(frame.payload for frame in removed).lower()
    assert b"could not delete" not in payload, payload.decode(errors="replace")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.requirement("FILE-015")
@pytest.mark.requirement("FILE-024")
@pytest.mark.requirement("FILE-025")
def test_directory_rename_is_visible_in_root_listing(sd_client, sd_fixture) -> None:
    """Exercises real FAT rename and enumeration through the public protocol."""
    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    suffix = uuid.uuid4().hex[:5].upper()
    source = f"/O{suffix}"
    destination = f"/N{suffix}"
    try:
        created = sd_client.exchange(
            GENERAL_COMMAND, f"mkdir {source}".encode("ascii"), 5.0
        )
        assert any(b"created directory" in frame.payload.lower() for frame in created)
        renamed = sd_client.exchange(
            GENERAL_COMMAND,
            f"mv {source} {destination}".encode("ascii"),
            5.0,
        )
        assert any(b"renamed" in frame.payload.lower() for frame in renamed)
        listed = sd_client.exchange(GENERAL_COMMAND, b"ls /", 5.0)
        listing = b"".join(frame.payload for frame in listed)
        assert destination[1:].encode("ascii") + b"/" in listing
        assert source[1:].encode("ascii") + b"/" not in listing
    finally:
        _remove(sd_client, destination)
        _remove(sd_client, source)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.requirement("HFT-004")
@pytest.mark.requirement("FILE-020")
def test_parent_traversal_remains_sandboxed_to_sd_root(sd_client, sd_fixture) -> None:
    """Proves traversal is normalized to the user-visible SD root on real VFS calls."""
    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    name = f"S{uuid.uuid4().hex[:6].upper()}"
    try:
        created = sd_client.exchange(
            GENERAL_COMMAND,
            f"mkdir /../../{name}".encode("ascii"),
            5.0,
        )
        message = b"".join(frame.payload for frame in created)
        assert f"created directory /{name}".encode("ascii") in message
        listed = sd_client.exchange(GENERAL_COMMAND, b"ls /", 5.0)
        assert name.encode("ascii") + b"/" in b"".join(
            frame.payload for frame in listed
        )
    finally:
        _remove(sd_client, f"/{name}")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("HFTU-001")
@pytest.mark.requirement("HFTU-003")
@pytest.mark.requirement("HFTU-004")
@pytest.mark.requirement("HFTU-005")
@pytest.mark.requirement("HFTU-007")
@pytest.mark.requirement("HFTD-001")
@pytest.mark.requirement("HFTD-004")
@pytest.mark.requirement("HFTD-005")
@pytest.mark.requirement("HFTD-006")
@pytest.mark.requirement("HFTD-009")
@pytest.mark.requirement("FILE-024")
@pytest.mark.requirement("FILE-025")
@pytest.mark.requirement("FILE-027")
@pytest.mark.requirement("FILE-029")
def test_file_roundtrip_md5_rename_and_delete(usb_client, sd_fixture) -> None:
    """Exercises file transfer and cache lifecycle through real target storage."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    suffix = uuid.uuid4().hex[:5].upper()
    source = f"/F{suffix}.BIN"
    destination = f"/R{suffix}.BIN"
    content = bytes((index * 37 + 11) & 0xFF for index in range(2500))
    digest = hashlib.md5(content).hexdigest().encode("ascii")
    try:
        upload_file(usb_client, source, content, block_size=700)
        md5_frames = usb_client.exchange(
            GENERAL_COMMAND, f"md5sum {source}".encode("ascii"), 5.0
        )
        assert digest in _payload(md5_frames).lower()
        assert download_file(usb_client, source) == content

        renamed = usb_client.exchange(
            GENERAL_COMMAND,
            f"mv {source} {destination}".encode("ascii"),
            5.0,
        )
        assert b"renamed" in _payload(renamed).lower()
        assert download_file(usb_client, destination) == content
    finally:
        _remove(usb_client, destination)
        _remove(usb_client, source)

    with pytest.raises(FileTransferError, match="failed to open file"):
        download_file(usb_client, destination)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.tcp
@pytest.mark.requirement("OWN-008")
@pytest.mark.requirement("TCP-005")
@pytest.mark.requirement("HFTU-005")
def test_large_tcp_upload_resumes_after_connection_loss(
    tcp_host: str, sd_fixture
) -> None:
    """Drops TCP mid-upload and requires the reused slot to request continuation."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    path = f"/N{uuid.uuid4().hex[:5].upper()}.BIN"
    block_size = 4096
    # Keep enough headroom for the mock's bounded diagnostic log while still
    # exercising multiple maximum-sized protocol blocks.
    content = bytes((index * 29 + 7) & 0xFF for index in range(64 * 1024))
    blocks = [
        content[index : index + block_size]
        for index in range(0, len(content), block_size)
    ]
    connection: socket.socket | None = None
    try:
        connection = socket.create_connection((tcp_host, 2222), timeout=4.0)
        # A successful upload start is intentionally silent; MD5 is the first
        # host transfer packet, so submit both before awaiting geometry.
        connection.sendall(
            encode_frame(FILE_COMMAND, f"upload {path}".encode("ascii"))
            + encode_frame(
                FILE_MD5, hashlib.md5(content).hexdigest().encode("ascii")
            )
        )
        assert _required_frame(
            receive_tcp_frames(connection, 4.0),
            FILE_GEOMETRY,
        )
        request = _required_frame(
            _tcp_transfer_exchange(
                connection, FILE_GEOMETRY, len(blocks).to_bytes(4, "big")
            ),
            FILE_DATA,
        )
        assert request is not None and int.from_bytes(request.payload, "big") == 1

        interruption_sequence = len(blocks) // 2
        for sequence in range(1, interruption_sequence + 1):
            responses = _tcp_transfer_exchange(
                connection,
                FILE_DATA,
                sequence.to_bytes(4, "big") + blocks[sequence - 1],
            )
            request = _required_frame(responses, FILE_DATA)
            assert request is not None, [
                (frame.frame_type, frame.payload) for frame in responses
            ]

        expected_sequence = interruption_sequence + 1
        assert int.from_bytes(request.payload, "big") == expected_sequence
        connection.shutdown(socket.SHUT_RDWR)
        connection.close()
        connection = None
        time.sleep(0.5)

        connection = socket.create_connection((tcp_host, 2222), timeout=4.0)
        resumed = _required_frame(
            receive_tcp_frames(connection, 4.0), FILE_DATA
        )
        assert resumed is not None
        assert int.from_bytes(resumed.payload, "big") == expected_sequence

        responses = []
        for sequence in range(expected_sequence, len(blocks) + 1):
            responses = _tcp_transfer_exchange(
                connection,
                FILE_DATA,
                sequence.to_bytes(4, "big") + blocks[sequence - 1],
            )
            if sequence < len(blocks):
                request = _required_frame(responses, FILE_DATA)
                assert request is not None, [
                    (frame.frame_type, frame.payload) for frame in responses
                ]
                assert int.from_bytes(request.payload, "big") == sequence + 1
        assert _required_frame(responses, FILE_COMPLETE) is not None

        connection.shutdown(socket.SHUT_RDWR)
        connection.close()
        connection = None
        digest_frames = TcpProtocolClient(tcp_host).exchange(
            GENERAL_COMMAND, f"md5sum {path}".encode("ascii"), 5.0
        )
        assert hashlib.md5(content).hexdigest().encode("ascii") in _payload(
            digest_frames
        ).lower()
    finally:
        if connection is not None:
            try:
                connection.close()
            except OSError:
                pass
        _remove(TcpProtocolClient(tcp_host), path)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("HFT-004")
@pytest.mark.requirement("FILE-001")
@pytest.mark.requirement("FILE-002")
@pytest.mark.requirement("FILE-003")
@pytest.mark.requirement("FILE-005")
def test_upload_parent_traversal_is_confined_to_sd(usb_client, sd_fixture) -> None:
    """Uploads through an escaping path and reads it from the clamped SD path."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    directory = f"T{uuid.uuid4().hex[:5].upper()}"
    canonical = f"/{directory}/SAFE.BIN"
    escaping = f"/../../{directory}/SAFE.BIN"
    content = b"sandbox-through-target-fat"
    try:
        created = usb_client.exchange(
            GENERAL_COMMAND, f"mkdir /{directory}".encode("ascii"), 5.0
        )
        assert b"created directory" in _payload(created).lower()
        upload_file(usb_client, escaping, content)
        assert download_file(usb_client, canonical) == content
    finally:
        _remove(usb_client, f"/{directory}")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("HFT-010")
@pytest.mark.requirement("HFT-011")
@pytest.mark.requirement("FILE-030")
@pytest.mark.requirement("FILE-031")
def test_gcodes_token_and_embedded_text_map_independently(usb_client, sd_fixture) -> None:
    """Checks exact gcodes path-token mapping without matching filename text."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    suffix = uuid.uuid4().hex[:4].upper()
    mapped_name = f"G{suffix}.BIN"
    embedded_name = f"XGCODE{suffix[:2]}.TXT"
    mapped_content = b"mapped-gcodes-token"
    embedded_content = b"ordinary-name-containing-gcode-text"
    try:
        usb_client.exchange(GENERAL_COMMAND, b"mkdir /gcodes", 5.0)
        upload_file(
            usb_client,
            f"/ignored/gcodes/{mapped_name}",
            mapped_content,
        )
        assert download_file(usb_client, f"/gcodes/{mapped_name}") == mapped_content

        upload_file(usb_client, f"/{embedded_name}", embedded_content)
        assert download_file(usb_client, f"/{embedded_name}") == embedded_content
    finally:
        _remove(usb_client, f"/gcodes/{mapped_name}")
        _remove(usb_client, f"/{embedded_name}")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("LOG-001")
@pytest.mark.requirement("LOG-006")
def test_serial_log_sentinel_mirrors_diagnostics(usb_client, sd_fixture) -> None:
    """Enables the opt-in mirror and retrieves one generated SD diagnostic."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    try:
        upload_file(usb_client, "/serial.log", b"")
        usb_client.exchange(
            GENERAL_COMMAND, b"md5sum /MISSING.BIN", timeout_seconds=5.0
        )
        time.sleep(0.2)
        log = download_file(usb_client, "/serial.log", verify_md5=False)
        assert b"SD access failed" in log
        assert b"MISSING.BIN" in log
    finally:
        _remove(usb_client, "/serial.log")
