"""Read-only physical checks for the native USB transport."""

from __future__ import annotations

import pytest

from tests.hardware.hil_protocol import GENERAL_COMMAND
from tools.provision_wifi import USB_PRODUCT_ID, USB_VENDOR_ID, _find_endpoints


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.usb
@pytest.mark.requirement("USB-001")
@pytest.mark.requirement("USB-002")
@pytest.mark.requirement("USB-003")
def test_native_usb_identity_and_bulk_endpoints(usb_device) -> None:
    assert usb_device.idVendor == USB_VENDOR_ID
    assert usb_device.idProduct == USB_PRODUCT_ID
    output, input_endpoint = _find_endpoints(usb_device)
    assert output.wMaxPacketSize == 64
    assert input_endpoint.wMaxPacketSize == 64


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.usb
@pytest.mark.requirement("USB-004")
@pytest.mark.requirement("USB-005")
@pytest.mark.requirement("USB-009")
def test_native_usb_read_only_round_trip(usb_client) -> None:
    frames = usb_client.exchange(
        GENERAL_COMMAND, b"ftype /sd/config.txt", timeout_seconds=4.0
    )
    assert frames, "firmware returned no valid frame for a read-only request"
    assert any(frame.payload for frame in frames), "all USB responses were empty"
