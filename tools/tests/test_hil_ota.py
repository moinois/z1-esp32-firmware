"""Tests status-driven reboot observation used by destructive HIL."""

from __future__ import annotations

import unittest
from unittest.mock import MagicMock, patch

from tests.hardware.hil_ota import wait_for_tcp_service_restart


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


if __name__ == "__main__":
    unittest.main()
