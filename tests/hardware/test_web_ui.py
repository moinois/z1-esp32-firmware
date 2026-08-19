"""Static web UI and configuration API checks on the installed SPIFFS image."""

from __future__ import annotations

import http.client
import json
import os
import time

import pytest

from tests.hardware.hil_file_transfer import download_file, upload_file
from tests.hardware.hil_protocol import GENERAL_COMMAND


def _request(
    host: str,
    method: str,
    path: str,
    body: bytes | None = None,
    content_type: str | None = None,
    timeout: float = 8.0,
) -> tuple[int, str, bytes]:
    """Performs one bounded HTTP request against the main web server."""

    headers = {"Content-Type": content_type} if content_type else {}
    connection = http.client.HTTPConnection(host, 80, timeout=timeout)
    try:
        connection.request(method, path, body=body, headers=headers)
        response = connection.getresponse()
        return response.status, response.getheader("Content-Type", ""), response.read()
    finally:
        connection.close()


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.requirement("WEB-011")
def test_installed_configuration_ui_assets(tcp_host: str) -> None:
    """Requires the packaged HTML, CSS, and JavaScript to be served verbatim."""

    for path, expected_type, marker in (
        ("/", "text/html", b"Current mainboard settings are loaded from the device"),
        ("/app.css", "text/css", b".setting-kind"),
        ("/app.js", "application/javascript", b"existing setting"),
    ):
        status, content_type, body = _request(tcp_host, "GET", path)
        assert status == 200
        assert content_type == expected_type
        assert marker in body


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.requirement("WEB-011")
def test_large_static_asset_completes_without_transport_stall(tcp_host: str) -> None:
    """Times an explicitly selected local asset without publishing its path."""

    asset_path = os.getenv("Z1_HIL_STATIC_ASSET")
    if not asset_path:
        pytest.skip("set Z1_HIL_STATIC_ASSET to an installed static asset path")
    if not asset_path.startswith("/"):
        pytest.fail("Z1_HIL_STATIC_ASSET must be an absolute HTTP path")
    timeout = float(os.getenv("Z1_HIL_STATIC_ASSET_TIMEOUT", "60"))
    if timeout <= 0.0:
        pytest.fail("Z1_HIL_STATIC_ASSET_TIMEOUT must be greater than zero")

    started = time.monotonic()
    status, _, body = _request(tcp_host, "GET", asset_path, timeout=timeout)
    elapsed = time.monotonic() - started

    assert status == 200
    assert body
    assert elapsed < timeout, f"static response stalled for {elapsed:.3f} seconds"


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.http
@pytest.mark.sd
@pytest.mark.requirement("CFG-010")
@pytest.mark.requirement("CFG-030")
def test_configuration_api_reads_validates_and_persists(
    tcp_host: str, usb_client, sd_fixture
) -> None:
    """Loads one fixture setting, updates it over HTTP, and checks SD bytes."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    fixture = b"# HIL web configuration\nMAINBOARD_HILVALUE=before\n"
    try:
        upload_file(usb_client, "/sd/config.txt", fixture)
        status, content_type, body = _request(tcp_host, "GET", "/api/config")
        assert status == 200
        assert content_type == "application/json"
        assert {item["key"]: item["value"] for item in json.loads(body)["settings"]}[
            "HILVALUE"
        ] == "before"

        request = json.dumps({"key": "HILVALUE", "value": "after"}).encode()
        status, content_type, body = _request(
            tcp_host, "POST", "/api/config", request, "application/json"
        )
        assert (status, content_type, body) == (200, "application/json", b'{"ok":true}')
        stored = download_file(usb_client, "/sd/config.txt")
        assert b"MAINBOARD_HILVALUE=after" in stored

        invalid = json.dumps({"key": "HILVALUE", "value": "bad\nline"}).encode()
        status, _, body = _request(
            tcp_host, "POST", "/api/config", invalid, "application/json"
        )
        assert status == 400
        assert body == b"Invalid configuration key or value"
    finally:
        try:
            usb_client.exchange(GENERAL_COMMAND, b"rm /sd/config.txt", 4.0)
        except Exception:
            pass
