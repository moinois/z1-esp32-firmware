#!/usr/bin/env python3
"""Provision station Wi-Fi credentials through the ESP32-S3 native USB port."""

from __future__ import annotations

import argparse
import sys
import time
from typing import Any, Iterable, Optional, Tuple

try:
    from tools.wifi_provision_protocol import (
        OPERATION_FAILURE,
        OPERATION_SUCCESS,
        TEXT_RESPONSE,
        build_wifi_command,
        decode_frames,
        encode_frame,
    )
except ModuleNotFoundError:
    from wifi_provision_protocol import (
        OPERATION_FAILURE,
        OPERATION_SUCCESS,
        TEXT_RESPONSE,
        build_wifi_command,
        decode_frames,
        encode_frame,
    )

USB_VENDOR_ID = 0x303A
USB_PRODUCT_ID = 0x4002
SINGLE_COMMAND = 0xA1
DEFAULT_TIMEOUT_MS = 1000
DEFAULT_TOTAL_TIMEOUT_SECONDS = 30.0


def _load_usb() -> Any:
    """Loads PyUSB lazily so protocol tests work without USB dependencies."""
    try:
        import usb.core  # type: ignore[import-not-found]
        import usb.util  # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError(
            "PyUSB is required; install it with 'python3 -m pip install pyusb'"
        ) from error
    return usb


def _find_endpoints(device: Any) -> Tuple[Any, Any]:
    """Returns the bulk OUT and IN endpoints of the vendor interface."""
    import usb.util  # type: ignore[import-not-found]

    try:
        configuration = device.get_active_configuration()
    except usb.core.USBError:
        device.set_configuration()
        configuration = device.get_active_configuration()
    interface = usb.util.find_descriptor(configuration, bInterfaceNumber=0)
    if interface is None:
        raise RuntimeError("Makera Z1 USB interface 0 was not found")
    endpoints = list(interface)
    output = usb.util.find_descriptor(
        interface,
        custom_match=lambda endpoint: usb.util.endpoint_direction(
            endpoint.bEndpointAddress
        )
        == usb.util.ENDPOINT_OUT,
    )
    input_endpoint = usb.util.find_descriptor(
        interface,
        custom_match=lambda endpoint: usb.util.endpoint_direction(
            endpoint.bEndpointAddress
        )
        == usb.util.ENDPOINT_IN,
    )
    if output is None or input_endpoint is None:
        raise RuntimeError(
            f"Expected bulk endpoints were not found (found {len(endpoints)})"
        )
    return output, input_endpoint


def _format_payload(payload: bytes) -> str:
    """Formats firmware text responses while retaining undecodable bytes."""
    return payload.decode("utf-8", errors="replace").rstrip("\r\n")


def provision_wifi(
    ssid: str,
    password: str,
    timeout_ms: int = DEFAULT_TIMEOUT_MS,
    total_timeout_seconds: float = DEFAULT_TOTAL_TIMEOUT_SECONDS,
) -> int:
    """Sends credentials and returns zero only after firmware reports success."""
    usb = _load_usb()
    device = usb.core.find(idVendor=USB_VENDOR_ID, idProduct=USB_PRODUCT_ID)
    if device is None:
        raise RuntimeError(
            "Makera Z1 native USB device was not found; connect the USB port"
        )

    output, input_endpoint = _find_endpoints(device)
    output.write(build_wifi_command(ssid, password), timeout=timeout_ms)

    received = b""
    deadline = time.monotonic() + total_timeout_seconds
    next_keepalive = time.monotonic() + 2.0
    while time.monotonic() < deadline:
        try:
            chunk = bytes(
                input_endpoint.read(
                    input_endpoint.wMaxPacketSize,
                    timeout=timeout_ms,
                )
            )
        except usb.core.USBTimeoutError:
            if time.monotonic() >= next_keepalive:
                try:
                    output.write(
                        encode_frame(SINGLE_COMMAND, b"?"), timeout=timeout_ms
                    )
                except usb.core.USBError:
                    pass
                next_keepalive = time.monotonic() + 2.0
            continue
        except usb.core.USBError as error:
            # Wi-Fi mode changes can briefly reset the native USB endpoint.
            # Reacquire the device instead of losing the pending response.
            if getattr(error, "errno", None) not in (5, 19, 13):
                raise
            time.sleep(0.1)
            device = usb.core.find(
                idVendor=USB_VENDOR_ID, idProduct=USB_PRODUCT_ID
            )
            if device is not None:
                try:
                    output, input_endpoint = _find_endpoints(device)
                    # A brief unmount clears firmware protocol activation;
                    # send a harmless status request to reactivate TX.
                    output.write(
                        encode_frame(SINGLE_COMMAND, b"?"), timeout=timeout_ms
                    )
                except usb.core.USBError:
                    pass
            continue
        received += chunk
        frames, received = decode_frames(received)
        for frame_type, payload in frames:
            if frame_type == TEXT_RESPONSE:
                print(_format_payload(payload))
            elif frame_type == OPERATION_SUCCESS:
                print(_format_payload(payload))
                return 0
            elif frame_type == OPERATION_FAILURE:
                print(_format_payload(payload), file=sys.stderr)
                return 1
    raise TimeoutError("Timed out waiting for the firmware WLAN response")


def _arguments(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    """Parses explicit credentials without storing them in source control."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ssid", help="target 2.4 GHz network name")
    parser.add_argument("password", help="target network password")
    parser.add_argument(
        "--timeout-ms",
        type=int,
        default=DEFAULT_TIMEOUT_MS,
        help="per USB transfer timeout (default: %(default)s)",
    )
    parser.add_argument(
        "--total-timeout",
        type=float,
        default=DEFAULT_TOTAL_TIMEOUT_SECONDS,
        help="overall response timeout in seconds (default: %(default)s)",
    )
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None) -> int:
    """Runs provisioning and converts expected failures into CLI diagnostics."""
    arguments = _arguments(argv)
    try:
        return provision_wifi(
            arguments.ssid,
            arguments.password,
            arguments.timeout_ms,
            arguments.total_timeout,
        )
    except (RuntimeError, TimeoutError, ValueError) as error:
        print(f"provisioning failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
