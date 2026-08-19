"""Read-only stream and cross-service checks for a physical camera module."""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import http.client
import os

import pytest

from tests.hardware.hil_protocol import GENERAL_COMMAND, SINGLE_COMMAND
from tests.hardware.hil_websocket import open_video_socket, receive_frame, send_text


def _require_controller() -> None:
    """Requires the controller used by the combined physical load case."""

    if os.getenv("Z1_HIL_CONTROLLER") != "1":
        pytest.skip("combined camera load requires Z1_HIL_CONTROLLER=1")


def _assert_jpeg(payload: bytes) -> None:
    """Checks the framing signatures of one physical camera JPEG."""

    assert len(payload) > 128, len(payload)
    assert payload.startswith(b"\xff\xd8"), payload[:8]
    assert payload.endswith(b"\xff\xd9"), payload[-8:]


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.camera
@pytest.mark.requirement("HW-040")
@pytest.mark.requirement("LIVE-001")
@pytest.mark.requirement("LIVE-005")
def test_physical_camera_streams_jpeg_and_recovers_after_disconnect(
    tcp_host: str, physical_camera_fixture,
) -> None:
    """Receives real frames, disconnects, then proves a successor can stream."""

    for _ in range(2):
        connection = open_video_socket(tcp_host)
        try:
            send_text(connection, b"start_stream")
            for _ in range(3):
                opcode, payload = receive_frame(connection)
                assert opcode == 2
                _assert_jpeg(payload)
            send_text(connection, b"stop_stream")
        finally:
            connection.close()


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.usb
@pytest.mark.controller
@pytest.mark.camera
@pytest.mark.requirement("LIVE-005")
@pytest.mark.requirement("USB-004")
@pytest.mark.requirement("UART-003")
def test_physical_camera_usb_http_and_controller_reads_coexist(
    tcp_host: str, usb_client, physical_camera_fixture
) -> None:
    """Exercises the physical stream while safe USB and HTTP reads are active."""

    _require_controller()
    connection = open_video_socket(tcp_host)
    try:
        send_text(connection, b"start_stream")
        opcode, payload = receive_frame(connection)
        assert opcode == 2
        _assert_jpeg(payload)

        def firmware_info() -> int:
            client = http.client.HTTPConnection(tcp_host, 80, timeout=5.0)
            try:
                client.request("GET", "/api/firmware/info")
                response = client.getresponse()
                response.read()
                return response.status
            finally:
                client.close()

        with ThreadPoolExecutor(max_workers=2) as executor:
            http_result = executor.submit(firmware_info)
            status_result = executor.submit(
                usb_client.exchange, SINGLE_COMMAND, b"?", 5.0
            )
            assert http_result.result() == 200
            assert any(frame.frame_type == 0x81 for frame in status_result.result())

        diagnostic = usb_client.exchange(GENERAL_COMMAND, b"diagnose", 5.0)
        assert any(frame.frame_type == 0x82 for frame in diagnostic)

        opcode, payload = receive_frame(connection)
        assert opcode == 2
        _assert_jpeg(payload)
    finally:
        connection.close()
