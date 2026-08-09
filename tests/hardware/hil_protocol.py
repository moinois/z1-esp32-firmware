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
DEFAULT_USB_QUIESCENCE_TIMEOUT_MS = 100
_STALE_HANDLE_ERRNOS = frozenset({5, 6, 19})
_STALE_HANDLE_BACKEND_CODES = frozenset({-1, -4})


@dataclass(frozen=True)
class ReceivedFrame:
    """One decoded response returned by a physical transport."""

    frame_type: int
    payload: bytes


class UsbTransferError(RuntimeError):
    """Adds transfer context and stale-handle guidance to a PyUSB failure."""

    def __init__(self, operation: str, error: Exception) -> None:
        """Classifies one failed bulk operation without hiding its root cause."""

        self.operation = operation
        self.errno = getattr(error, "errno", None)
        self.backend_error_code = getattr(error, "backend_error_code", None)
        self.stale_handle_candidate = (
            self.errno in _STALE_HANDLE_ERRNOS
            or self.backend_error_code in _STALE_HANDLE_BACKEND_CODES
        )
        guidance = (
            "; the device may still be enumerated, retry with a fresh USB handle"
            if self.stale_handle_candidate
            else ""
        )
        super().__init__(
            f"USB {operation} failed (errno={self.errno}, "
            f"backend={self.backend_error_code}): {error}{guidance}"
        )


class UsbProtocolClient:
    """Sends bounded request frames over the native USB vendor interface."""

    def __init__(
        self,
        device: Any,
        timeout_ms: int = DEFAULT_TIMEOUT_MS,
        quiescence_timeout_ms: int = DEFAULT_USB_QUIESCENCE_TIMEOUT_MS,
    ) -> None:
        """Claims vendor bulk endpoints from an already discovered USB device."""

        if timeout_ms <= 0 or quiescence_timeout_ms <= 0:
            raise ValueError("USB timeouts must be positive")
        self.device = device
        self.timeout_ms = timeout_ms
        self.quiescence_timeout_ms = min(timeout_ms, quiescence_timeout_ms)
        self.output, self.input = _find_endpoints(device)

    def exchange(
        self, frame_type: int, payload: bytes, timeout_seconds: float = 3.0
    ) -> List[ReceivedFrame]:
        """Sends one request and collects complete responses until quiescence."""

        self.send(frame_type, payload)
        return self.receive(timeout_seconds)

    def exchange_until(
        self,
        frame_type: int,
        payload: bytes,
        terminal_types: frozenset[int],
        timeout_seconds: float = 3.0,
    ) -> List[ReceivedFrame]:
        """Sends a request and returns as soon as a terminal frame is decoded."""

        self.send(frame_type, payload)
        return self.receive(timeout_seconds, terminal_types=terminal_types)

    def send(self, frame_type: int, payload: bytes) -> None:
        """Writes one encoded frame without waiting for a target response."""

        usb = _load_usb()
        try:
            self.output.write(encode_frame(frame_type, payload), timeout=self.timeout_ms)
        except usb.core.USBError as error:
            raise UsbTransferError("bulk write", error) from error

    def send_encoded(self, encoded_frames: bytes) -> None:
        """Writes pre-encoded frames, allowing tests to fragment or batch traffic."""

        usb = _load_usb()
        try:
            self.output.write(encoded_frames, timeout=self.timeout_ms)
        except usb.core.USBError as error:
            raise UsbTransferError("bulk write", error) from error

    def receive(
        self,
        timeout_seconds: float = 3.0,
        *,
        terminal_types: frozenset[int] | None = None,
    ) -> List[ReceivedFrame]:
        """Receives frames until a terminal type, quiescence, or the deadline."""

        usb = _load_usb()
        deadline = time.monotonic() + timeout_seconds
        remainder = b""
        received: List[ReceivedFrame] = []
        while time.monotonic() < deadline:
            try:
                read_timeout_ms = (
                    self.quiescence_timeout_ms if received else self.timeout_ms
                )
                chunk = bytes(
                    self.input.read(self.input.wMaxPacketSize, timeout=read_timeout_ms)
                )
            except usb.core.USBTimeoutError:
                if received and terminal_types is None:
                    break
                continue
            except usb.core.USBError as error:
                raise UsbTransferError("bulk read", error) from error
            frames, remainder = decode_frames(remainder + chunk)
            received.extend(ReceivedFrame(kind, body) for kind, body in frames)
            if terminal_types is not None and any(
                frame.frame_type in terminal_types for frame in received
            ):
                break
        return received

    def close(self) -> None:
        """Releases the libusb handle so a reset device can be reclaimed."""

        usb_util = __import__("usb.util", fromlist=["dispose_resources"])
        usb_util.dispose_resources(self.device)


class TcpProtocolClient:
    """Sends one request through the target's framed TCP control service."""

    def __init__(self, host: str, port: int = 2222) -> None:
        """Retains the control endpoint used for a fresh connection per exchange."""

        self.host = host
        self.port = port

    def exchange(
        self, frame_type: int, payload: bytes, timeout_seconds: float = 3.0
    ) -> List[ReceivedFrame]:
        """Opens a connection, sends one frame, and preserves terminal replies."""

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

    def exchange_until(
        self,
        frame_type: int,
        payload: bytes,
        terminal_types: frozenset[int],
        timeout_seconds: float = 3.0,
    ) -> List[ReceivedFrame]:
        """Returns once a known terminal response makes quiescence unnecessary."""

        remainder = b""
        received: List[ReceivedFrame] = []
        deadline = time.monotonic() + timeout_seconds
        with socket.create_connection(
            (self.host, self.port), timeout=timeout_seconds
        ) as connection:
            try:
                connection.settimeout(0.25)
                connection.sendall(encode_frame(frame_type, payload))
                while time.monotonic() < deadline:
                    try:
                        chunk = connection.recv(8192)
                    except socket.timeout:
                        continue
                    if not chunk:
                        break
                    frames, remainder = decode_frames(remainder + chunk)
                    received.extend(
                        ReceivedFrame(kind, body) for kind, body in frames
                    )
                    if any(
                        frame.frame_type in terminal_types for frame in received
                    ):
                        break
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
