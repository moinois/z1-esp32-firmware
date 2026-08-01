"""Physical transport helpers built on the tested host protocol codec."""

from __future__ import annotations

import socket
import time
from dataclasses import dataclass
from typing import Any, List, Tuple

from tools.provision_wifi import (
    DEFAULT_TIMEOUT_MS,
    USB_PRODUCT_ID,
    USB_VENDOR_ID,
    _find_endpoints,
    _load_usb,
)
from tools.wifi_provision_protocol import decode_frames, encode_frame

SINGLE_COMMAND = 0xA1
GENERAL_COMMAND = 0xA2


@dataclass(frozen=True)
class ReceivedFrame:
    """One decoded response returned by a physical transport."""

    frame_type: int
    payload: bytes


class UsbProtocolClient:
    """Sends bounded request frames over the native USB vendor interface."""

    def __init__(self, device: Any, timeout_ms: int = DEFAULT_TIMEOUT_MS) -> None:
        self.device = device
        self.timeout_ms = timeout_ms
        self.output, self.input = _find_endpoints(device)

    def exchange(
        self, frame_type: int, payload: bytes, timeout_seconds: float = 3.0
    ) -> List[ReceivedFrame]:
        self.send(frame_type, payload)
        return self.receive(timeout_seconds)

    def send(self, frame_type: int, payload: bytes) -> None:
        """Writes one encoded frame without waiting for a target response."""

        self.output.write(encode_frame(frame_type, payload), timeout=self.timeout_ms)

    def send_encoded(self, encoded_frames: bytes) -> None:
        """Writes pre-encoded frames, allowing tests to fragment or batch traffic."""

        self.output.write(encoded_frames, timeout=self.timeout_ms)

    def receive(self, timeout_seconds: float = 3.0) -> List[ReceivedFrame]:
        """Receives all complete response frames available before the deadline."""

        usb = _load_usb()
        deadline = time.monotonic() + timeout_seconds
        remainder = b""
        received: List[ReceivedFrame] = []
        while time.monotonic() < deadline:
            try:
                chunk = bytes(
                    self.input.read(self.input.wMaxPacketSize, timeout=self.timeout_ms)
                )
            except usb.core.USBTimeoutError:
                if received:
                    break
                continue
            frames, remainder = decode_frames(remainder + chunk)
            received.extend(ReceivedFrame(kind, body) for kind, body in frames)
        return received

    def close(self) -> None:
        """Releases the libusb handle so a reset device can be reclaimed."""

        usb_util = __import__("usb.util", fromlist=["dispose_resources"])
        usb_util.dispose_resources(self.device)


class TcpProtocolClient:
    """Sends one request through the target's framed TCP control service."""

    def __init__(self, host: str, port: int = 2222) -> None:
        self.host = host
        self.port = port

    def exchange(
        self, frame_type: int, payload: bytes, timeout_seconds: float = 3.0
    ) -> List[ReceivedFrame]:
        remainder = b""
        received: List[ReceivedFrame] = []
        deadline = time.monotonic() + timeout_seconds
        with socket.create_connection((self.host, self.port), timeout=timeout_seconds) as connection:
            try:
                connection.settimeout(0.25)
                connection.sendall(encode_frame(frame_type, payload))
                while time.monotonic() < deadline:
                    try:
                        chunk = connection.recv(8192)
                    except socket.timeout:
                        if received:
                            break
                        continue
                    except ConnectionResetError:
                        # A target may close immediately after its terminal
                        # response. Preserve already decoded evidence while
                        # still surfacing resets that arrived before a reply.
                        if received:
                            break
                        raise
                    if not chunk:
                        break
                    frames, remainder = decode_frames(remainder + chunk)
                    received.extend(ReceivedFrame(kind, body) for kind, body in frames)
            finally:
                try:
                    connection.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass
        return received


def receive_tcp_frames(connection: socket.socket,
                       timeout_seconds: float = 3.0) -> List[ReceivedFrame]:
    """Receives all currently available framed responses from one open socket."""
    remainder = b""
    received: List[ReceivedFrame] = []
    deadline = time.monotonic() + timeout_seconds
    connection.settimeout(0.25)
    while time.monotonic() < deadline:
        try:
            chunk = connection.recv(8192)
        except socket.timeout:
            if received:
                break
            continue
        if not chunk:
            break
        frames, remainder = decode_frames(remainder + chunk)
        received.extend(ReceivedFrame(kind, body) for kind, body in frames)
    return received


def find_native_usb_device() -> Tuple[Any | None, str | None]:
    """Finds the native USB interface or returns a precise skip reason."""

    try:
        usb = _load_usb()
    except RuntimeError as error:
        return None, str(error)
    try:
        device = usb.core.find(idVendor=USB_VENDOR_ID, idProduct=USB_PRODUCT_ID)
    except usb.core.USBError as error:
        return None, f"native USB discovery failed: {error}"
    if device is None:
        return None, "Makera Z1 native USB device 303a:4002 was not detected"
    return device, None
