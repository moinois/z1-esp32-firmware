"""WebSocket lifecycle, preemption, and concurrency checks for mock camera."""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import http.client
import json
import os

import pytest

from tests.hardware.hil_protocol import GENERAL_COMMAND, TcpProtocolClient
from tests.hardware.hil_websocket import (
    open_video_socket as _open_video_socket,
    receive_frame as _receive_frame,
    send_text as _send_text,
)


JPEG = b"\xff\xd8\xff\xd9"


def _set_resolution(host: str, value: int) -> None:
    """Applies one camera frame-size number through the public HTTP API."""

    connection = http.client.HTTPConnection(host, 80, timeout=5.0)
    body = json.dumps({"resolution": value}).encode("ascii")
    connection.request(
        "POST",
        "/api/camera/resolution",
        body=body,
        headers={"Content-Type": "application/json"},
    )
    response = connection.getresponse()
    payload = response.read()
    connection.close()
    assert response.status == 200, (response.status, payload)


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.camera
@pytest.mark.requirement("LIVE-001")
@pytest.mark.requirement("LIVE-005")
@pytest.mark.requirement("LIVE-006")
@pytest.mark.requirement("LIVE-008")
def test_mock_camera_repeated_start_stop_resolution_and_disconnect(
    tcp_host: str,
) -> None:
    """Cycles stream ownership and resolution without leaking the video task."""

    if os.getenv("Z1_HIL_MOCK_CAMERA") != "1":
        pytest.skip("mock camera not declared with Z1_HIL_MOCK_CAMERA=1")
    for cycle, resolution in enumerate((1, 10, 15, 4, 12)):
        # LIVE-010 initializes camera capability lazily on the first video
        # WebSocket request. The resolution endpoint must therefore be used
        # after opening the socket; before that, sensor rejection is normative.
        connection = _open_video_socket(tcp_host)
        try:
            _set_resolution(tcp_host, resolution)
            _send_text(connection, b"start_stream")
            for _ in range(3):
                opcode, payload = _receive_frame(connection)
                assert opcode == 2
                assert payload == JPEG
            _send_text(connection, b"stop_stream")
        finally:
            connection.close()

        # Exercise another service after every disconnect so a leaked or stale
        # video owner cannot hide behind the next WebSocket upgrade.
        frames = TcpProtocolClient(tcp_host).exchange(
            GENERAL_COMMAND, b"sn-get", 5.0
        )
        assert frames, f"TCP did not recover after camera cycle {cycle + 1}"


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.camera
@pytest.mark.tcp
@pytest.mark.requirement("LIVE-005")
@pytest.mark.requirement("MEDIA-003")
def test_mock_camera_second_client_preempts_first_and_services_remain_live(
    tcp_host: str,
) -> None:
    """Verifies cross-socket preemption and concurrent HTTP/TCP responsiveness."""

    if os.getenv("Z1_HIL_MOCK_CAMERA") != "1":
        pytest.skip("mock camera not declared with Z1_HIL_MOCK_CAMERA=1")
    first = _open_video_socket(tcp_host)
    second = _open_video_socket(tcp_host)
    try:
        _send_text(first, b"start_stream")
        assert _receive_frame(first) == (2, JPEG)
        _send_text(second, b"start_stream")
        assert _receive_frame(second) == (2, JPEG)
        # One or more JPEG frames may already be queued by the asynchronous
        # producer before the preemption control frame reaches the old socket.
        for _ in range(10):
            opcode, payload = _receive_frame(first)
            if opcode == 1:
                break
            assert (opcode, payload) == (2, JPEG)
        else:
            pytest.fail("preempted live client received no text control frame")
        assert opcode == 1
        assert b'"ns":"vlive"' in payload
        assert b'"rsp":"preempted"' in payload

        def http_diagnostics() -> int:
            connection = http.client.HTTPConnection(tcp_host, 80, timeout=5.0)
            connection.request("GET", "/api/wifi/diagnostics")
            response = connection.getresponse()
            response.read()
            connection.close()
            return response.status

        with ThreadPoolExecutor(max_workers=2) as executor:
            http_result = executor.submit(http_diagnostics)
            tcp_result = executor.submit(
                TcpProtocolClient(tcp_host).exchange,
                GENERAL_COMMAND,
                b"sys-time",
                5.0,
            )
            assert http_result.result() == 200
            assert tcp_result.result()
        assert _receive_frame(second) == (2, JPEG)
    finally:
        first.close()
        second.close()

    successor = _open_video_socket(tcp_host)
    try:
        _send_text(successor, b"start_stream")
        assert _receive_frame(successor) == (2, JPEG)
    finally:
        successor.close()
