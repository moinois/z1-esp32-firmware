"""HTTP helpers shared by destructive target reboot and update checks."""

from __future__ import annotations

import http.client


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
