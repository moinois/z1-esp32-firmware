"""Explicitly gated Wi-Fi persistence and native USB reset validation."""

from __future__ import annotations

import os
import time

import pytest

from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    UsbProtocolClient,
    find_native_usb_device,
)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.wifi
@pytest.mark.requirement("NET-010")
@pytest.mark.requirement("NET-022")
def test_wifi_credentials_can_be_applied_and_saved(usb_client) -> None:
    ssid = os.getenv("Z1_HIL_WIFI_SSID")
    password = os.getenv("Z1_HIL_WIFI_PASSWORD")
    if not ssid or password is None:
        pytest.skip("set Z1_HIL_WIFI_SSID and Z1_HIL_WIFI_PASSWORD")
    command = f"wlan -s {ssid} {password}".encode("utf-8")
    frames = usb_client.exchange(GENERAL_COMMAND, command, 30.0)
    assert frames
    payload = b"\n".join(frame.payload for frame in frames).lower()
    assert b"failed" not in payload


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.requirement("USB-005")
def test_native_usb_reset_reenumerates_and_recovers(usb_device) -> None:
    if os.getenv("Z1_HIL_USB_RESET") != "1":
        pytest.skip("set Z1_HIL_USB_RESET=1 to permit native USB reset")
    usb_device.reset()
    deadline = time.monotonic() + 15.0
    while time.monotonic() < deadline:
        device, _ = find_native_usb_device()
        if device is not None:
            frames = UsbProtocolClient(device).exchange(
                GENERAL_COMMAND, b"ftype /", 4.0
            )
            assert frames
            return
        time.sleep(0.25)
    pytest.fail("native USB device did not re-enumerate after reset")
