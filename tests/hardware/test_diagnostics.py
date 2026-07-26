"""Optional read-only discovery of the ESP32-S3 diagnostic serial interface."""

from __future__ import annotations

import os

import pytest


def _diagnostic_port() -> str | None:
    configured = os.getenv("Z1_HIL_SERIAL")
    if configured:
        return configured
    try:
        from serial.tools import list_ports  # type: ignore[import-not-found]
    except ImportError:
        return None
    candidates = [
        port.device
        for port in list_ports.comports()
        if "usbmodem" in port.device.lower()
        or "esp32" in (port.description or "").lower()
    ]
    return candidates[0] if len(candidates) == 1 else None


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.diagnostics
def test_diagnostic_serial_port_is_detectable() -> None:
    port = _diagnostic_port()
    if port is None:
        pytest.skip(
            "diagnostic serial port not uniquely detected; install pyserial or set Z1_HIL_SERIAL"
        )
    assert os.path.exists(port), f"configured diagnostic port does not exist: {port}"
