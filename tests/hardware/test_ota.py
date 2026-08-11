"""Explicitly gated destructive OTA transport checks."""

from __future__ import annotations

import os
from pathlib import Path
import socket
import time

import pytest

from tests.hardware.hil_ota import (
    multipart_upload,
    open_usb_before_restart,
    wait_for_usb_service_restart,
)


@pytest.mark.hardware
@pytest.mark.destructive
@pytest.mark.http
@pytest.mark.requirement("WEBUP-020")
@pytest.mark.requirement("WEBUP-022")
def test_web_volume_image_can_be_installed(tcp_host: str) -> None:
    image_name = os.getenv("Z1_HIL_SPIFFS_IMAGE")
    if not image_name:
        pytest.skip("set Z1_HIL_SPIFFS_IMAGE to a valid SPIFFS image")
    image_path = Path(image_name)
    if not image_path.is_file():
        pytest.skip(f"SPIFFS image fixture does not exist: {image_path}")
    previous_usb = open_usb_before_restart()
    status, body = multipart_upload(
        tcp_host, "/updateffs", image_path.read_bytes(), image_path.name
    )
    assert status == 200
    assert body == b"UI upgrade finished. The system will reboot in 2 seconds..."
    wait_for_usb_service_restart(previous_usb).close()


@pytest.mark.hardware
@pytest.mark.destructive
@pytest.mark.http
@pytest.mark.requirement("WEBUP-002")
@pytest.mark.requirement("WEBUP-010")
def test_ota_survives_receive_timeout(tcp_host: str) -> None:
    image_name = os.getenv("Z1_HIL_OTA_IMAGE")
    if not image_name:
        pytest.skip("set Z1_HIL_OTA_IMAGE to the application image to install")
    image_path = Path(image_name)
    if not image_path.is_file():
        pytest.skip(f"OTA image fixture does not exist: {image_path}")

    image = image_path.read_bytes()
    boundary = "z1-hil-timeout-recovery"
    prefix = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="firmware"; '
        f'filename="{image_path.name}"\r\n'
        "Content-Type: application/octet-stream\r\n\r\n"
    ).encode("ascii")
    suffix = f"\r\n--{boundary}--\r\n".encode("ascii")
    content_length = len(prefix) + len(image) + len(suffix)
    headers = (
        "POST /update HTTP/1.1\r\n"
        f"Host: {tcp_host}\r\n"
        f"Content-Type: multipart/form-data; boundary={boundary}\r\n"
        f"Content-Length: {content_length}\r\n"
        "Connection: close\r\n\r\n"
    ).encode("ascii")

    previous_usb = open_usb_before_restart()
    try:
        connection = socket.create_connection((tcp_host, 80), timeout=10.0)
    except OSError as error:
        pytest.skip(f"Makera Z1 HTTP service not detected at {tcp_host}:80: {error}")
    with connection:
        connection.settimeout(180.0)
        initial_image_bytes = min(4096, len(image))
        connection.sendall(headers + prefix + image[:initial_image_bytes])
        # The configured HTTP receive timeout is five seconds. This gap must
        # exercise at least one timeout before the valid upload continues.
        time.sleep(7.0)
        connection.sendall(image[initial_image_bytes:] + suffix)
        response = bytearray()
        while True:
            block = connection.recv(4096)
            if not block:
                break
            response.extend(block)
            header_end = response.find(b"\r\n\r\n")
            if header_end < 0:
                continue
            headers_received = bytes(response[:header_end]).decode(
                "iso-8859-1"
            )
            content_length = next(
                (
                    int(line.split(":", 1)[1].strip())
                    for line in headers_received.split("\r\n")
                    if line.lower().startswith("content-length:")
                ),
                None,
            )
            if content_length is not None and len(response) >= (
                header_end + 4 + content_length
            ):
                break

    status_line = bytes(response).partition(b"\r\n")[0]
    assert status_line.startswith(b"HTTP/1.1 200 ")
    assert response.endswith(
        b"Firmware upgrade finished. The system will reboot in 2 seconds..."
    )
    wait_for_usb_service_restart(previous_usb).close()
