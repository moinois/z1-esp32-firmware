"""Destructive NVS and Wi-Fi persistence checks across a real target reboot."""

from __future__ import annotations

import json
import os
from pathlib import Path
import re
import time
import urllib.request

import pytest

from tests.hardware.hil_ota import (
    multipart_upload,
    open_usb_before_restart,
    wait_for_usb_service_restart,
)
from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    UsbProtocolClient,
    find_native_usb_device,
)


_runtime_pattern = re.compile(
    rb"sys-time-data = ([^,]+),(\d+),(\d+)"
)


def _payload(client, command: bytes, timeout: float = 4.0) -> bytes:
    """Returns all response payloads for one transport command."""

    return b"\n".join(
        frame.payload
        for frame in client.exchange(GENERAL_COMMAND, command, timeout)
    )


def _runtime(client) -> tuple[bytes, int, int]:
    """Parses the persisted first-boot and counter values from `sys-time`."""

    payload = _payload(client, b"sys-time")
    match = _runtime_pattern.search(payload)
    assert match is not None, payload
    return match.group(1), int(match.group(2)), int(match.group(3))


def _wait_for_usb(timeout_seconds: float = 20.0) -> UsbProtocolClient:
    """Returns a newly enumerated native USB protocol client after reboot."""

    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        device, _ = find_native_usb_device()
        if device is not None:
            try:
                client = UsbProtocolClient(device)
                if client.exchange(GENERAL_COMMAND, b"sn-get", 1.0):
                    return client
            except Exception:
                pass
        time.sleep(0.25)
    pytest.fail("native USB protocol did not recover after OTA reboot")


@pytest.mark.hardware
@pytest.mark.destructive
@pytest.mark.tcp
@pytest.mark.wifi
@pytest.mark.controller
@pytest.mark.requirement("RUN-010")
@pytest.mark.requirement("RUN-030")
@pytest.mark.requirement("RUN-031")
@pytest.mark.requirement("RUN-032")
@pytest.mark.requirement("RUN-043")
@pytest.mark.requirement("NET-010")
@pytest.mark.requirement("NET-017")
def test_runtime_identity_and_wifi_persist_across_ota_reboot(tcp_host: str) -> None:
    """Recreates first boot, reinstalls the image, and verifies NVS recovery."""

    if os.getenv("Z1_HIL_MOCK_CONTROLLER") != "1":
        pytest.skip("first-boot injection requires Z1_HIL_MOCK_CONTROLLER=1")
    image_name = os.getenv("Z1_HIL_OTA_IMAGE")
    if not image_name:
        pytest.skip("set Z1_HIL_OTA_IMAGE to the application image to reinstall")
    image_path = Path(image_name)
    if not image_path.is_file():
        pytest.skip(f"OTA image fixture does not exist: {image_path}")

    client = _wait_for_usb()
    assert b"clearftm ok" in _payload(client, b"clearftm").lower()
    assert _runtime(client)[0] == b"null"

    requested_time = int(time.time())
    client.exchange(
        GENERAL_COMMAND,
        f"mock-time {requested_time}".encode("ascii"),
        1.0,
    )
    deadline = time.monotonic() + 5.0
    before = _runtime(client)
    while before[0] == b"null" and time.monotonic() < deadline:
        time.sleep(0.1)
        before = _runtime(client)
    assert before[0] != b"null"
    serial_before = _payload(client, b"sn-get")
    assert b"sn get failed" not in serial_before.lower()
    assert b"sn = " in serial_before.lower()

    previous_usb = open_usb_before_restart()
    status, body = multipart_upload(
        tcp_host,
        "/update",
        image_path.read_bytes(),
        image_path.name,
    )
    assert status == 200
    assert body == b"Firmware upgrade finished. The system will reboot in 2 seconds..."
    restored = wait_for_usb_service_restart(previous_usb)
    after = _runtime(restored)
    assert after[0] == before[0]
    assert after[1] >= before[1]
    assert after[2] == before[2]
    assert _payload(restored, b"sn-get") == serial_before

    with urllib.request.urlopen(
        f"http://{tcp_host}/api/wifi/diagnostics", timeout=5.0
    ) as response:
        diagnostics = json.load(response)
    assert diagnostics["associations"] >= 1
    assert diagnostics["addresses_acquired"] >= 1
