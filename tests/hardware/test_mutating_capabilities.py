"""Explicitly gated Wi-Fi persistence and native USB reset validation."""

from __future__ import annotations

import hashlib
import os
import socket
import time
import uuid

import pytest

from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    UsbProtocolClient,
    find_native_usb_device,
)
from tests.hardware.hil_file_transfer import (
    FILE_COMMAND,
    FILE_COMPLETE,
    FILE_DATA,
    FILE_GEOMETRY,
    FILE_MD5,
    download_file,
)
from tools.wifi_provision_protocol import encode_frame


def _require_usb_reset() -> None:
    """Skips tests that need permission to reset the native USB connection."""

    if os.getenv("Z1_HIL_USB_RESET") != "1":
        pytest.skip("set Z1_HIL_USB_RESET=1 to permit native USB reset")


def _client_after_reenumeration(timeout_seconds: float = 15.0) -> UsbProtocolClient:
    """Returns a fresh client after the native USB device becomes available."""

    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        device, _ = find_native_usb_device()
        if device is not None:
            try:
                return UsbProtocolClient(device)
            except Exception:
                # macOS can publish descriptors before the interface is ready
                # to be claimed after a bus reset.
                pass
        time.sleep(0.25)
    pytest.fail("native USB device did not re-enumerate after reset")


def _exchange_after_reenumeration(
    frame_type: int,
    payload: bytes,
    timeout_seconds: float = 15.0,
) -> tuple[UsbProtocolClient, list]:
    """Retries a request until the re-enumerated protocol endpoint is responsive."""

    deadline = time.monotonic() + timeout_seconds
    client = _client_after_reenumeration(timeout_seconds)
    while time.monotonic() < deadline:
        try:
            frames = client.exchange(frame_type, payload, 1.0)
            if frames:
                return client, frames
        except Exception:
            client = _client_after_reenumeration(max(0.5, deadline - time.monotonic()))
        time.sleep(0.1)
    pytest.fail("native USB protocol did not become responsive after reset")


def _required_frame(frames, frame_type: int):
    """Returns one required frame while retaining useful assertion output."""

    frame = next((item for item in frames if item.frame_type == frame_type), None)
    assert frame is not None, [hex(item.frame_type) for item in frames]
    return frame


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.wifi
@pytest.mark.requirement("NET-010")
@pytest.mark.requirement("NET-022")
def test_wifi_credentials_can_be_applied_and_saved(usb_client, tcp_host: str) -> None:
    ssid = os.getenv("Z1_HIL_WIFI_SSID")
    password = os.getenv("Z1_HIL_WIFI_PASSWORD")
    if not ssid or password is None:
        pytest.skip("set Z1_HIL_WIFI_SSID and Z1_HIL_WIFI_PASSWORD")
    command = f"wlan -s {ssid} {password}".encode("utf-8")
    frames = usb_client.exchange(GENERAL_COMMAND, command, 30.0)
    assert frames
    payload = b"\n".join(frame.payload for frame in frames).lower()
    assert b"failed" not in payload

    # Applying even unchanged credentials can briefly reassociate the station.
    # Do not let that deliberate transition leak into later network HIL cases.
    deadline = time.monotonic() + 60.0
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((tcp_host, 80), timeout=1.0):
                break
        except OSError:
            time.sleep(0.25)
    else:
        pytest.fail("network services did not recover after applying credentials")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.requirement("USB-005")
def test_native_usb_reset_reenumerates_and_recovers(usb_device) -> None:
    _require_usb_reset()
    usb_device.reset()
    client, frames = _exchange_after_reenumeration(GENERAL_COMMAND, b"ftype /")
    try:
        assert frames
    finally:
        client.close()


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.requirement("USB-005")
@pytest.mark.requirement("USB-006")
def test_native_usb_disconnect_discards_partial_receive_frame(usb_device) -> None:
    """A bus reset between frame fragments must not poison the next frame."""

    _require_usb_reset()
    client = UsbProtocolClient(usb_device)
    frame = encode_frame(GENERAL_COMMAND, b"ftype /")
    client.send_encoded(frame[: len(frame) // 2])
    usb_device.reset()

    recovered_client, recovered = _exchange_after_reenumeration(
        GENERAL_COMMAND, b"ftype /"
    )
    try:
        assert any(b"ftype" in item.payload.lower() for item in recovered), recovered
    finally:
        recovered_client.close()


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.sd
@pytest.mark.requirement("USB-005")
@pytest.mark.requirement("OWN-008")
def test_native_usb_upload_continues_after_disconnect(usb_device, sd_fixture) -> None:
    """Continues an owned upload at the pending sequence after a bus reset."""

    _require_usb_reset()
    path = f"/U{uuid.uuid4().hex[:5].upper()}.BIN"
    content = bytes((index * 31 + 9) & 0xFF for index in range(4096))
    client = UsbProtocolClient(usb_device)
    try:
        # A successful upload start is silent, so batch it with the first
        # expected host packet instead of waiting for a response in between.
        client.send_encoded(
            encode_frame(FILE_COMMAND, f"upload {path}".encode("ascii"))
            + encode_frame(
                FILE_MD5, hashlib.md5(content).hexdigest().encode("ascii")
            )
        )
        _required_frame(client.receive(4.0), FILE_GEOMETRY)
        request = _required_frame(
            client.exchange(FILE_GEOMETRY, (1).to_bytes(4, "big"), 4.0),
            FILE_DATA,
        )
        assert request.payload == (1).to_bytes(4, "big")

        usb_device.reset()
        client, completed = _exchange_after_reenumeration(
            FILE_DATA, (1).to_bytes(4, "big") + content
        )
        _required_frame(completed, FILE_COMPLETE)
        assert download_file(client, path) == content
    finally:
        try:
            client.exchange(GENERAL_COMMAND, f"rm {path}".encode("ascii"), 4.0)
        except Exception:
            pass
        client.close()
