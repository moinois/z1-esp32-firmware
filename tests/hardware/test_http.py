"""Read-only physical checks for the main HTTP service."""

from __future__ import annotations

import http.client
import json
import socket

import pytest


def _request(host: str, method: str, path: str, body: bytes | None = None,
             headers: dict[str, str] | None = None) -> tuple[int, str, bytes]:
    connection = http.client.HTTPConnection(host, 80, timeout=5.0)
    try:
        connection.request(method, path, body=body, headers=headers or {})
        response = connection.getresponse()
        return response.status, response.getheader("Content-Type", ""), response.read()
    except OSError as error:
        pytest.skip(f"Makera Z1 HTTP service not detected at {host}:80: {error}")
    finally:
        connection.close()


def _websocket_status(host: str, path: str) -> int:
    request = (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: {host}:82\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n"
    ).encode("ascii")
    try:
        with socket.create_connection((host, 82), timeout=5.0) as connection:
            connection.sendall(request)
            response = connection.recv(512)
            if response.startswith(b"HTTP/1.1 101"):
                connection.sendall(b"\x88\x80\x00\x00\x00\x00")
    except OSError as error:
        pytest.skip(f"Makera Z1 video HTTP service not detected at {host}:82: {error}")
    status_line = response.split(b"\r\n", 1)[0]
    return int(status_line.split(b" ", 2)[1])


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.requirement("WEB-013")
def test_http_firmware_identity(tcp_host: str) -> None:
    status, content_type, body = _request(tcp_host, "GET", "/api/firmware/info")
    assert status == 200
    assert content_type == "application/json"
    assert body == (
        b'{"version":"0.1.13","build_date":"2026.08.05",'
        b'"idf_ver":"v5.4.1"}'
    )


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.requirement("NET-DIAG-001")
def test_http_wifi_diagnostics(tcp_host: str) -> None:
    status, content_type, body = _request(
        tcp_host, "GET", "/api/wifi/diagnostics"
    )
    assert status == 200
    assert content_type == "application/json"
    diagnostics = json.loads(body)
    assert set(diagnostics) == {
        "connected", "rssi_dbm", "channel", "authentication",
        "ipv4_address", "station_starts", "associations",
        "disconnections", "addresses_acquired", "addresses_lost",
        "last_disconnect_reason", "reset_reason", "recent_events",
    }
    assert isinstance(diagnostics["connected"], bool)
    assert isinstance(diagnostics["recent_events"], str)
    for counter in (
        "station_starts", "associations", "disconnections",
        "addresses_acquired", "addresses_lost",
    ):
        assert isinstance(diagnostics[counter], int)
        assert diagnostics[counter] >= 0
    if diagnostics["connected"]:
        assert -127 <= diagnostics["rssi_dbm"] <= 0
        assert 1 <= diagnostics["channel"] <= 14


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.requirement("WEB-011")
def test_http_missing_static_file(tcp_host: str) -> None:
    status, content_type, body = _request(tcp_host, "GET", "/__hil_missing__.txt")
    assert status == 404
    assert content_type == "text/html"
    assert body == b"Nothing matches the given URI"


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.requirement("WEB-020")
def test_http_camera_rejects_invalid_json(tcp_host: str) -> None:
    status, content_type, body = _request(
        tcp_host,
        "POST",
        "/api/camera/resolution",
        b"not-json",
        {"Content-Type": "application/json"},
    )
    assert status == 400
    assert content_type == "text/html"
    assert body == b"Invalid JSON"


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.requirement("WEB-003")
@pytest.mark.parametrize("path", ["/ws_video", "/ws_preview"])
def test_video_server_accepts_websocket_upgrade(tcp_host: str, path: str) -> None:
    assert _websocket_status(tcp_host, path) == 101
