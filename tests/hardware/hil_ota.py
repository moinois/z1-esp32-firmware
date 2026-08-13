"""HTTP helpers shared by destructive target reboot and update checks."""

from __future__ import annotations

import http.client
import socket
import time

import pytest

from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    UsbProtocolClient,
    find_native_usb_device,
)

_TARGET_MULTIPART_BLOCK_BYTES = 1024
_UPLOAD_CHUNK_BYTES = 16 * 1024
_INITIAL_BODY_DELAY_SECONDS = 3.0


def _multipart_body(boundary: str, image: bytes, filename: str) -> bytes:
    """Builds one first part ending exactly after the image.

    WEBUP-003 defines boundary detection independently for each receive block.
    Multipart end markers receive no special treatment and may either discard
    preceding image bytes or become image content. The specification also says
    transport end completes input, so this fixture intentionally ends its
    Content-Length after the exact image and emits no closing marker. Keeping
    the opening header short also keeps its terminator in the first receive
    block under the target's supported request domain.
    """

    prefix = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="firmware"; '
        f'filename="{filename}"\r\n'
        "Content-Type: application/octet-stream\r\n\r\n"
    ).encode("ascii")
    return prefix + image


def multipart_upload(
    host: str,
    endpoint: str,
    image: bytes,
    filename: str,
    *,
    timeout_seconds: float = 180.0,
    maximum_attempts: int = 3,
    retry_delay_seconds: float = 1.0,
) -> tuple[int, bytes]:
    """Uploads one complete binary field with bounded transport retries.

    HTTP cannot resume a body after its TCP connection is lost. A retry starts
    a new request from byte zero after closing the partial transaction. HTTP
    responses are returned to the test unchanged and are never retried, so a
    firmware validation failure cannot be hidden by fixture recovery.
    """

    if maximum_attempts < 1:
        raise ValueError("maximum_attempts must be positive")

    boundary = "z1-hil-complete-upload"
    body = _multipart_body(boundary, image, filename)
    last_error: OSError | None = None
    for attempt in range(maximum_attempts):
        connection = http.client.HTTPConnection(
            host, 80, timeout=timeout_seconds
        )
        try:
            connection.putrequest("POST", endpoint)
            connection.putheader(
                "Content-Type", f"multipart/form-data; boundary={boundary}"
            )
            connection.putheader("Content-Length", str(len(body)))
            connection.endheaders()
            # The target erases the OTA partition before its first body read.
            # Let that work finish so the aligned opening-header block is not
            # split inside the HTTP server's already-buffered request bytes.
            time.sleep(_INITIAL_BODY_DELAY_SECONDS)
            first_block_end = min(_TARGET_MULTIPART_BLOCK_BYTES, len(body))
            connection.send(body[:first_block_end])
            for offset in range(first_block_end, len(body), _UPLOAD_CHUNK_BYTES):
                connection.send(body[offset : offset + _UPLOAD_CHUNK_BYTES])
            response = connection.getresponse()
            return response.status, response.read()
        except OSError as error:
            last_error = error
            if attempt + 1 >= maximum_attempts:
                raise
        finally:
            connection.close()
        time.sleep(retry_delay_seconds)
    assert last_error is not None
    raise last_error


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


def open_usb_before_restart() -> UsbProtocolClient:
    """Opens the application USB transport that must be invalidated by reboot."""

    device, reason = find_native_usb_device()
    if device is None:
        pytest.skip(reason or "native USB device unavailable before reboot")
    return UsbProtocolClient(device)


def wait_for_usb_service_restart(
    previous: UsbProtocolClient,
    *,
    timeout_seconds: float = 45.0,
    poll_interval_seconds: float = 0.1,
) -> UsbProtocolClient:
    """Requires old-handle failure, bus absence, and a new protocol reply.

    An endpoint exchange can fail transiently while the pre-reboot device is
    still enumerated. Treating that failure alone as disconnect lets the same
    old USB instance satisfy the recovery probe before the scheduled reboot
    has even begun.
    """

    deadline = time.monotonic() + timeout_seconds
    old_handle_failed = False
    observed_bus_absence = False
    while time.monotonic() < deadline:
        if not old_handle_failed:
            try:
                previous.exchange(GENERAL_COMMAND, b"sn-get", 0.25)
            except Exception:
                old_handle_failed = True
                previous.close()
            if not old_handle_failed:
                time.sleep(poll_interval_seconds)
                continue

        device, _ = find_native_usb_device()
        if device is None:
            observed_bus_absence = True
        elif observed_bus_absence:
            try:
                current = UsbProtocolClient(device)
                if current.exchange(GENERAL_COMMAND, b"sn-get", 1.0):
                    return current
                current.close()
            except Exception:
                pass
        time.sleep(poll_interval_seconds)
    if not old_handle_failed:
        state = "invalidate the old handle"
    elif not observed_bus_absence:
        state = "disappear from the USB bus"
    else:
        state = "re-enumerate and answer"
    pytest.fail(f"native USB did not {state} across target reboot")
