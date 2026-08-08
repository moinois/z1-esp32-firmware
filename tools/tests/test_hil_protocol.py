"""Tests USB HIL transport timing and diagnostic error classification."""

from __future__ import annotations

import unittest
from unittest.mock import patch

from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    UsbProtocolClient,
    UsbTransferError,
)
from tools.wifi_provision_protocol import encode_frame


class _UsbTimeoutError(Exception):
    """Represents the timeout type exposed by a minimal fake PyUSB module."""


class _UsbError(Exception):
    """Carries the error attributes used to classify a failed bulk transfer."""

    def __init__(self, message: str, *, errno: int, backend_error_code: int) -> None:
        super().__init__(message)
        self.errno = errno
        self.backend_error_code = backend_error_code


class _FakeUsb:
    """Provides only the PyUSB exception namespace required by the client."""

    class core:
        USBTimeoutError = _UsbTimeoutError
        USBError = _UsbError


class _InputEndpoint:
    """Returns scripted reads while retaining the requested timeout values."""

    wMaxPacketSize = 64

    def __init__(self, reads: list[bytes | Exception]) -> None:
        self.reads = list(reads)
        self.timeouts: list[int] = []

    def read(self, _size: int, *, timeout: int) -> bytes:
        self.timeouts.append(timeout)
        result = self.reads.pop(0)
        if isinstance(result, Exception):
            raise result
        return result


class _OutputEndpoint:
    """Records writes or raises one configured transport error."""

    def __init__(self, error: Exception | None = None) -> None:
        self.error = error

    def write(self, _data: bytes, *, timeout: int) -> None:
        if self.error is not None:
            raise self.error


def _client(input_endpoint: _InputEndpoint, output_endpoint: _OutputEndpoint) -> UsbProtocolClient:
    """Constructs a client around deterministic endpoints without real USB."""

    client = UsbProtocolClient.__new__(UsbProtocolClient)
    client.device = object()
    client.timeout_ms = 1000
    client.quiescence_timeout_ms = 100
    client.output = output_endpoint
    client.input = input_endpoint
    return client


class HilProtocolTests(unittest.TestCase):
    """Verifies bounded response completion and actionable USB failures."""

    @patch("tests.hardware.hil_protocol._load_usb", return_value=_FakeUsb)
    def test_receive_uses_short_quiescence_timeout_after_first_frame(self, _load) -> None:
        endpoint = _InputEndpoint(
            [
                encode_frame(GENERAL_COMMAND, b"ok"),
                _UsbTimeoutError("quiet"),
            ]
        )
        frames = _client(endpoint, _OutputEndpoint()).receive(3.0)

        self.assertEqual([frame.payload for frame in frames], [b"ok"])
        self.assertEqual(endpoint.timeouts, [1000, 100])

    @patch("tests.hardware.hil_protocol._load_usb", return_value=_FakeUsb)
    def test_receive_keeps_normal_timeout_until_a_complete_frame_exists(self, _load) -> None:
        encoded = encode_frame(GENERAL_COMMAND, b"split")
        endpoint = _InputEndpoint(
            [
                encoded[:4],
                encoded[4:],
                _UsbTimeoutError("quiet"),
            ]
        )
        frames = _client(endpoint, _OutputEndpoint()).receive(3.0)

        self.assertEqual([frame.payload for frame in frames], [b"split"])
        self.assertEqual(endpoint.timeouts, [1000, 1000, 100])

    @patch("tests.hardware.hil_protocol._load_usb", return_value=_FakeUsb)
    def test_write_io_error_is_classified_as_stale_handle_candidate(self, _load) -> None:
        client = _client(
            _InputEndpoint([]),
            _OutputEndpoint(_UsbError("I/O error", errno=5, backend_error_code=-1)),
        )

        with self.assertRaises(UsbTransferError) as captured:
            client.send(GENERAL_COMMAND, b"version")

        self.assertEqual(captured.exception.operation, "bulk write")
        self.assertTrue(captured.exception.stale_handle_candidate)
        self.assertIn("fresh USB handle", str(captured.exception))


if __name__ == "__main__":
    unittest.main()
