"""Recoverable HTTP concurrency and interrupted-request validation."""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import http.client
import socket

import pytest


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
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.requirement("WEBUP-002")
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
    status, body = _firmware_info(tcp_host)
    assert status == 200
    assert body
