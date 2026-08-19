"""Read-only preview playback checks using an existing physical-SD AVI file."""

from __future__ import annotations

import json
import os

import pytest

from tests.hardware.hil_websocket import open_video_socket, receive_frame, send_text


def _preview_file() -> str:
    """Returns the operator-declared existing AVI without assuming private media."""

    path = os.getenv("Z1_HIL_PREVIEW_FILE")
    if not path:
        pytest.skip("set Z1_HIL_PREVIEW_FILE to an existing /sd/videos AVI")
    if not path.startswith("/sd/videos/") or ".." in path:
        pytest.fail("Z1_HIL_PREVIEW_FILE must be below /sd/videos without '..'")
    return path


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.sd
@pytest.mark.camera
@pytest.mark.requirement("PREV-010")
@pytest.mark.requirement("PREV-012")
@pytest.mark.requirement("PREV-013")
@pytest.mark.requirement("PREV-014")
@pytest.mark.requirement("PREV-028")
def test_physical_avi_preview_returns_metadata_and_jpeg_frames(
    tcp_host: str,
) -> None:
    """Opens existing media, validates metadata/JPEG transport, and stops it."""

    path = _preview_file()
    connection = open_video_socket(tcp_host, "/ws_preview")
    session_id = ""
    binary_frames = 0
    try:
        request_sequence = 77
        request = json.dumps(
            {"ns": "vpreview", "cmd": "open", "seq": request_sequence, "path": path},
            separators=(",", ":"),
        ).encode("utf-8")
        send_text(connection, request)

        saw_metadata = False
        for _ in range(12):
            opcode, payload = receive_frame(connection)
            if opcode == 1:
                response = json.loads(payload)
                if response.get("rsp") == "open":
                    assert response["seq"] == request_sequence
                    assert response["err"] == 0
                    assert response["path"] == path
                    assert response["filename"] == path.rsplit("/", 1)[-1]
                    assert response["total_frames"] > 0
                    assert response["frame_period_us"] > 0
                    assert response["width"] > 0 and response["height"] > 0
                    session_id = response["session_id"]
                elif response.get("rsp") == "meta":
                    assert response["seq"] == request_sequence
                    assert response["err"] == 0
                    assert response["stream"] == "jpeg"
                    assert response["first_frame_index"] == 0
                    saw_metadata = True
            elif opcode == 2:
                assert payload.startswith(b"\xff\xd8")
                assert payload.endswith(b"\xff\xd9")
                binary_frames += 1
                if binary_frames == 3:
                    break

        assert session_id
        assert saw_metadata
        assert binary_frames == 3
        stop = json.dumps(
            {
                "ns": "vpreview",
                "cmd": "stop",
                "seq": request_sequence + 1,
                "session_id": session_id,
            },
            separators=(",", ":"),
        ).encode("utf-8")
        send_text(connection, stop)
    finally:
        connection.close()
