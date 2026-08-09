"""HTTP helpers shared by destructive target reboot and update checks."""

from __future__ import annotations

import http.client
import socket
import time

import pytest


def multipart_upload(
    host: str,
    endpoint: str,
    image: bytes,
    filename: str,
    *,
    timeout_seconds: float = 180.0,
) -> tuple[int, bytes]:
    """Uploads one complete binary multipart field to a target endpoint."""

    boundary = "z1-hil-complete-upload"
    body = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="firmware"; '
        f'filename="{filename}"\r\n'
        "Content-Type: application/octet-stream\r\n\r\n"
    ).encode("ascii") + image + f"\r\n--{boundary}--\r\n".encode("ascii")
    connection = http.client.HTTPConnection(
        host, 80, timeout=timeout_seconds
    )
    try:
        connection.request(
            "POST",
            endpoint,
            body,
            {"Content-Type": f"multipart/form-data; boundary={boundary}"},
        )
        response = connection.getresponse()
        return response.status, response.read()
    finally:
        connection.close()


def wait_for_tcp_service_restart(
    host: str,
    port: int,
    *,
    timeout_seconds: float = 45.0,
    poll_interval_seconds: float = 0.1,
) -> None:
    """Observes a service disappear and return across a scheduled reboot.

    Update endpoints reply before their reboot timer expires. Requiring both
    edges prevents a following destructive test from accidentally connecting
    to the old boot, without assuming a fixed target boot duration.
    """

    deadline = time.monotonic() + timeout_seconds
    observed_outage = False
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((host, port), timeout=0.25):
                available = True
        except OSError:
            available = False
        if not available:
            observed_outage = True
        elif observed_outage:
            return
        time.sleep(poll_interval_seconds)
    state = "recover" if observed_outage else "stop before reboot"
    pytest.fail(f"target TCP service did not {state} on port {port}")
