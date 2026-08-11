"""Tests status-driven reboot observation used by destructive HIL."""

from __future__ import annotations

import unittest
from unittest.mock import MagicMock, patch

from tests.hardware.hil_ota import (
    wait_for_tcp_service_restart,
    wait_for_usb_service_restart,
)


class HilOtaTests(unittest.TestCase):
    """Verifies that reboot completion requires both service-state edges."""

    @patch("tests.hardware.hil_ota.socket.create_connection")
    def test_restart_waits_for_outage_before_accepting_recovery(
        self, create_connection
    ) -> None:
        available = MagicMock()
        create_connection.side_effect = [available, OSError("down"), available]

        wait_for_tcp_service_restart(
            "192.0.2.1", 80, timeout_seconds=1.0, poll_interval_seconds=0.0
        )

        self.assertEqual(create_connection.call_count, 3)

    @patch("tests.hardware.hil_ota.UsbProtocolClient")
    @patch("tests.hardware.hil_ota.find_native_usb_device")
    def test_usb_restart_requires_old_handle_failure_and_new_protocol_reply(
        self, find_device, client_type
    ) -> None:
        previous = MagicMock()
        previous.exchange.side_effect = OSError("detached")
        current = MagicMock()
        current.exchange.return_value = [object()]
        client_type.return_value = current
        find_device.return_value = (object(), None)

        result = wait_for_usb_service_restart(
            previous, timeout_seconds=1.0, poll_interval_seconds=0.0
        )

        self.assertIs(result, current)
        previous.close.assert_called_once_with()
        current.exchange.assert_called_once()

    @patch("tests.hardware.hil_ota.UsbProtocolClient")
    @patch("tests.hardware.hil_ota.find_native_usb_device")
    def test_usb_restart_closes_an_unresponsive_reenumerated_handle_before_retry(
        self, find_device, client_type
    ) -> None:
        previous = MagicMock()
        previous.exchange.side_effect = OSError("detached")
        silent = MagicMock()
        silent.exchange.return_value = []
        responsive = MagicMock()
        responsive.exchange.return_value = [object()]
        client_type.side_effect = [silent, responsive]
        find_device.side_effect = [(object(), None), (object(), None)]

        result = wait_for_usb_service_restart(
            previous, timeout_seconds=1.0, poll_interval_seconds=0.0
        )

        self.assertIs(result, responsive)
        silent.close.assert_called_once_with()


if __name__ == "__main__":
    unittest.main()
