"""Recoverable HTTP concurrency and interrupted-request validation."""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import http.client
import socket
import time

import pytest

from tests.hardware.hil_protocol import GENERAL_COMMAND, TcpProtocolClient


def _firmware_info(host: str) -> tuple[int, bytes]:
    connection = http.client.HTTPConnection(host, 80, timeout=5.0)
    try:
        connection.request("GET", "/api/firmware/info")
        response = connection.getresponse()
        return response.status, response.read()
    finally:
        connection.close()


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.requirement("WEB-001")
def test_http_repeated_concurrent_requests_remain_available(tcp_host: str) -> None:
    with ThreadPoolExecutor(max_workers=4) as executor:
        results = list(executor.map(lambda _: _firmware_info(tcp_host), range(12)))
    assert all(status == 200 and body for status, body in results)


@pytest.mark.hardware
@pytest.mark.destructive
@pytest.mark.http
@pytest.mark.requirement("WEBUP-002")
@pytest.mark.requirement("WEBUP-004")
def test_interrupted_multipart_request_does_not_wedge_http(tcp_host: str) -> None:
    request = (
        "POST /update HTTP/1.1\r\n"
        f"Host: {tcp_host}\r\n"
        "Content-Type: multipart/form-data; boundary=z1-interrupted\r\n"
        "Content-Length: 100000\r\n"
        "Connection: close\r\n\r\n"
        "--z1-interrupted\r\n"
    ).encode("ascii")
    with socket.create_connection((tcp_host, 80), timeout=5.0) as connection:
        connection.sendall(request)
    deadline = time.monotonic() + 15.0
    last_error: OSError | None = None
    while time.monotonic() < deadline:
        try:
            status, body = _firmware_info(tcp_host)
        except OSError as error:
            last_error = error
            continue
        assert status == 200
        assert body
        break
    else:
        pytest.fail(
            "HTTP did not recover after the interrupted multipart receive "
            f"window: {last_error}"
        )


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.tcp
@pytest.mark.usb
@pytest.mark.requirement("WEB-001")
@pytest.mark.requirement("OWN-001")
def test_http_usb_and_tcp_remain_available_together(
    tcp_host: str, usb_client
) -> None:
    """Exercises all three host services concurrently for repeated waves."""

    def usb_request() -> bool:
        return bool(usb_client.exchange(GENERAL_COMMAND, b"ftype /", 4.0))

    def tcp_request() -> bool:
        return bool(
            TcpProtocolClient(tcp_host).exchange(
                GENERAL_COMMAND, b"ftype /", 4.0
            )
        )

    with ThreadPoolExecutor(max_workers=3) as executor:
        for _ in range(8):
            http_result = executor.submit(_firmware_info, tcp_host)
            usb_result = executor.submit(usb_request)
            tcp_result = executor.submit(tcp_request)
            assert http_result.result()[0] == 200
            assert usb_result.result()
            assert tcp_result.result()
