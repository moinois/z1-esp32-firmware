"""Tests bounded host-side recovery used by destructive OTA fixtures."""

from __future__ import annotations

import pytest
import unittest
from unittest.mock import MagicMock, patch

from tests.hardware import hil_ota


class _Response:
    status = 200

    def read(self) -> bytes:
        return b"accepted"


class _Connection:
    attempts = 0
    closed = 0
    sent_bodies: list[bytes] = []

    def __init__(self, host: str, port: int, timeout: float) -> None:
        assert host == "target"
        assert port == 80
        assert timeout == 2.0

    def putrequest(self, method: str, endpoint: str) -> None:
        assert method == "POST"
        assert endpoint == "/update"

    def putheader(self, name: str, value: str) -> None:
        if name == "Content-Type":
            assert "boundary=" in value

    def endheaders(self) -> None:
        pass

    def send(self, body: bytes) -> None:
        type(self).sent_bodies.append(body)
        type(self).attempts += 1
        if type(self).attempts == 1:
            raise TimeoutError("temporary network loss")

    def getresponse(self) -> _Response:
        return _Response()

    def close(self) -> None:
        type(self).closed += 1


def test_multipart_upload_retries_transport_failure_from_byte_zero(monkeypatch) -> None:
    _Connection.attempts = 0
    _Connection.closed = 0
    _Connection.sent_bodies = []
    monkeypatch.setattr(hil_ota.http.client, "HTTPConnection", _Connection)
    monkeypatch.setattr(hil_ota.time, "sleep", lambda _: None)

    assert hil_ota.multipart_upload(
        "target", "/update", b"image", "firmware.bin",
        timeout_seconds=2.0, maximum_attempts=2,
    ) == (200, b"accepted")
    assert _Connection.attempts == 2
    assert _Connection.closed == 2
    assert b"firmware.bin" in b"".join(_Connection.sent_bodies)
    assert _Connection.sent_bodies[0] == _Connection.sent_bodies[1]


def test_multipart_upload_sends_large_body_in_bounded_chunks(monkeypatch) -> None:
    _Connection.attempts = 1
    _Connection.closed = 0
    _Connection.sent_bodies = []
    monkeypatch.setattr(hil_ota.http.client, "HTTPConnection", _Connection)
    monkeypatch.setattr(hil_ota.time, "sleep", lambda _: None)

    hil_ota.multipart_upload(
        "target", "/update",
        b"x" * (hil_ota._TARGET_MULTIPART_BLOCK_BYTES * 2),
        "firmware.bin", timeout_seconds=2.0, maximum_attempts=1,
    )

    assert len(_Connection.sent_bodies) == 2
    assert len(_Connection.sent_bodies[0]) == hil_ota._TARGET_MULTIPART_BLOCK_BYTES
    assert len(_Connection.sent_bodies[1]) <= hil_ota._UPLOAD_CHUNK_BYTES


@pytest.mark.parametrize("image_size", [1, 1023, 1024, 1692672])
def test_multipart_body_ends_with_exact_image_without_closing_marker(image_size) -> None:
    boundary = "fixture-boundary"
    body = hil_ota._multipart_body(boundary, b"x" * image_size, "firmware.bin")
    image_offset = len(body) - image_size

    assert body[image_offset:] == b"x" * image_size
    assert body[:image_offset].endswith(b"\r\n\r\n")
    assert f"--{boundary}--".encode("ascii") not in body


def test_multipart_upload_rejects_empty_attempt_budget() -> None:
    with pytest.raises(ValueError, match="maximum_attempts must be positive"):
        hil_ota.multipart_upload(
            "target", "/update", b"image", "firmware.bin",
            maximum_attempts=0,
        )


class HilOtaRestartTests(unittest.TestCase):
    """Verifies that reboot completion requires every service-state edge."""

    @patch("tests.hardware.hil_ota.socket.create_connection")
    def test_tcp_restart_waits_for_outage_before_accepting_recovery(
        self, create_connection
    ) -> None:
        available = MagicMock()
        create_connection.side_effect = [available, OSError("down"), available]

        hil_ota.wait_for_tcp_service_restart(
            "192.0.2.1", 80, timeout_seconds=1.0,
            poll_interval_seconds=0.0,
        )

        self.assertEqual(create_connection.call_count, 3)

    @patch("tests.hardware.hil_ota.UsbProtocolClient")
    @patch("tests.hardware.hil_ota.find_native_usb_device")
    def test_usb_restart_requires_bus_absence_before_new_protocol_reply(
        self, find_device, client_type
    ) -> None:
        previous = MagicMock()
        previous.exchange.side_effect = OSError("detached")
        current = MagicMock()
        current.exchange.return_value = [object()]
        client_type.return_value = current
        find_device.side_effect = [(None, None), (object(), None)]

        result = hil_ota.wait_for_usb_service_restart(
            previous, timeout_seconds=1.0, poll_interval_seconds=0.0
        )

        self.assertIs(result, current)
        previous.close.assert_called_once_with()
        current.exchange.assert_called_once()

    @patch("tests.hardware.hil_ota.UsbProtocolClient")
    @patch("tests.hardware.hil_ota.find_native_usb_device")
    def test_usb_restart_closes_unresponsive_new_handle_before_retry(
        self, find_device, client_type
    ) -> None:
        previous = MagicMock()
        previous.exchange.side_effect = OSError("detached")
        silent = MagicMock()
        silent.exchange.return_value = []
        responsive = MagicMock()
        responsive.exchange.return_value = [object()]
        client_type.side_effect = [silent, responsive]
        find_device.side_effect = [
            (None, None), (object(), None), (object(), None)
        ]

        result = hil_ota.wait_for_usb_service_restart(
            previous, timeout_seconds=1.0, poll_interval_seconds=0.0
        )

        self.assertIs(result, responsive)
        silent.close.assert_called_once_with()
