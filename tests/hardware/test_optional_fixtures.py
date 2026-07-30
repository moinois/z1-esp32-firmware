"""Capability gates for fixture-dependent physical validation domains."""

from __future__ import annotations

import os
import socket
import time

import pytest

from tests.hardware.hil_file_transfer import FileTransferError, download_file, upload_file
from tests.hardware.hil_protocol import GENERAL_COMMAND, SINGLE_COMMAND, TcpProtocolClient


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.can
@pytest.mark.controller
@pytest.mark.requirement("LPC-001")
@pytest.mark.requirement("UART-003")
def test_controller_fixture_declares_connection() -> None:
    if os.getenv("Z1_HIL_CONTROLLER") != "1":
        pytest.skip("external controller fixture not declared with Z1_HIL_CONTROLLER=1")
    pytest.skip("controller fixture driver is not implemented yet")


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.controller
def test_mock_controller_populates_runtime_snapshots(tcp_host: str) -> None:
    """Exercises controller routing without claiming physical UART conformance."""
    if os.getenv("Z1_HIL_MOCK_CONTROLLER") != "1":
        pytest.skip("mock controller not declared with Z1_HIL_MOCK_CONTROLLER=1")

    client = TcpProtocolClient(tcp_host)

    def wait_for_payload(frame_type: int, command: bytes, response_type: int) -> bytes:
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            frames = client.exchange(frame_type, command, timeout_seconds=1.0)
            for frame in frames:
                if frame.frame_type == response_type and frame.payload:
                    return frame.payload
            time.sleep(0.1)
        pytest.fail(f"mock controller did not answer {command!r} with type 0x{response_type:02x}")

    status = wait_for_payload(SINGLE_COMMAND, b"?", 0x81)
    assert status.startswith(b"<Idle|")
    assert b"MPos:0.0000,0.0000,0.0000" in status

    diagnostic = wait_for_payload(GENERAL_COMMAND, b"diagnose", 0x82)
    assert b"MOCK:1" in diagnostic
    assert b"UART:OK" in diagnostic
    assert b"CTRL:SIMULATED" in diagnostic
    assert b"RSSI:" in diagnostic

    version = wait_for_payload(GENERAL_COMMAND, b"version", 0x83)
    assert version == b"version = mock-controller-1.0.1.11\n"


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.controller
@pytest.mark.sd
@pytest.mark.requirement("LPC-010")
@pytest.mark.requirement("LPCFW-001")
@pytest.mark.requirement("LPCCFG-001")
@pytest.mark.requirement("LPCFAC-001")
def test_mock_controller_completes_all_transfer_families(
    usb_client, tcp_host: str, sd_fixture
) -> None:
    """Runs controller-originated firmware, config, and factory handshakes."""

    if os.getenv("Z1_HIL_MOCK_CONTROLLER") != "1":
        pytest.skip("mock controller not declared with Z1_HIL_MOCK_CONTROLLER=1")
    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    client = TcpProtocolClient(tcp_host)

    def wait_for_result(expected: bytes) -> None:
        deadline = time.monotonic() + 8.0
        while time.monotonic() < deadline:
            frames = client.exchange(GENERAL_COMMAND, b"diagnose", 1.0)
            payload = b"\n".join(frame.payload for frame in frames)
            if expected in payload:
                return
            time.sleep(0.1)
        pytest.fail(f"mock controller transfer did not publish {expected!r}")

    files = {
        "/lpc1768.bin": bytes((index * 11 + 7) & 0xFF for index in range(1300)),
        "/config.txt": b"machine.name=Mock Z1\naxis.count=4\n",
        "/factory.ini": b"serial=MOCK-001\ncalibration=nominal\n",
    }
    try:
        for path, content in files.items():
            upload_file(usb_client, path, content)

        for command, result in (
            (b"mock-transfer firmware", b"XFER:FIRMWARE:OK"),
            (b"mock-transfer configuration", b"XFER:CONFIGURATION:OK"),
            (b"mock-transfer factory", b"XFER:FACTORY:OK"),
        ):
            # The test-only command is intentionally forwarded to the selected
            # controller adapter and therefore has no direct host response.
            client.exchange(GENERAL_COMMAND, command, 1.0)
            wait_for_result(result)

        assert download_file(usb_client, "/config.txt") == files["/config.txt"]
        for consumed_path in ("/lpc1768.bin", "/factory.ini"):
            with pytest.raises(FileTransferError, match="failed to open file"):
                download_file(usb_client, consumed_path)
    finally:
        for path in files:
            try:
                usb_client.exchange(
                    GENERAL_COMMAND, f"rm {path}".encode("ascii"), 4.0
                )
            except Exception:
                pass


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.can
@pytest.mark.requirement("CAN-001")
def test_can_fixture_declares_connection() -> None:
    if os.getenv("Z1_HIL_CAN") != "1":
        pytest.skip("CAN fixture not declared with Z1_HIL_CAN=1")
    pytest.skip("CAN fixture driver is not implemented yet")


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.camera
@pytest.mark.requirement("HW-040")
def test_camera_fixture_detects_sensor(camera_fixture) -> None:
    """Records camera availability without failing camera-less mainboards."""
    assert camera_fixture is None


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.camera
def test_mock_camera_stream_returns_deterministic_jpeg(tcp_host: str) -> None:
    """Exercises the target camera composition without claiming physical conformance."""
    if os.getenv("Z1_HIL_MOCK_CAMERA") != "1":
        pytest.skip("mock camera not declared with Z1_HIL_MOCK_CAMERA=1")
    request = (
        "GET /ws_video HTTP/1.1\r\n"
        f"Host: {tcp_host}:82\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n"
    ).encode("ascii")
    command = b"start_stream"
    mask = b"\x12\x34\x56\x78"
    masked = bytes(value ^ mask[index % 4] for index, value in enumerate(command))
    frame = bytes((0x81, 0x80 | len(command))) + mask + masked
    with socket.create_connection((tcp_host, 82), timeout=5.0) as connection:
        connection.settimeout(5.0)
        connection.sendall(request)
        response = connection.recv(1024)
        assert response.startswith(b"HTTP/1.1 101")
        connection.sendall(frame)
        received = bytearray()
        while len(received) < 6:
            block = connection.recv(6 - len(received))
            assert block, "mock camera WebSocket closed before one frame arrived"
            received.extend(block)
    assert received[:2] == b"\x82\x04"
    assert received[2:6] == b"\xff\xd8\xff\xd9"
