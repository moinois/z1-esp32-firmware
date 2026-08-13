"""Diagnostic serial discovery and explicitly gated reset/boot validation."""

from __future__ import annotations

import os
from pathlib import Path
import time

import pytest


def _diagnostic_port() -> str | None:
    configured = os.getenv("Z1_HIL_SERIAL")
    if configured:
        return configured if os.path.exists(configured) else None
    try:
        from serial.tools import list_ports  # type: ignore[import-not-found]
    except ImportError:
        return None
    # The ESP ROM USB-JTAG port (303a:4001) is a bootloader transport, not an
    # application diagnostics stream. Prefer the firmware CDC port (303a:4002)
    # and never toggle reset lines on the bootloader-only port here.
    candidates = [
        port.device
        for port in list_ports.comports()
        if port.vid == 0x303A and port.pid == 0x4002
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


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.diagnostics
@pytest.mark.requirement("BOOT-001")
@pytest.mark.requirement("DIAG-001")
def test_reset_emits_healthy_boot_diagnostics() -> None:
    """Resets through DTR/RTS and rejects fatal diagnostics in the boot log."""
    port = _diagnostic_port()
    if port is None:
        pytest.skip(
            "diagnostic serial port not uniquely detected; set Z1_HIL_SERIAL"
        )
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError:
        pytest.skip("install pyserial to capture diagnostic boot output")

    captured = bytearray()
    with serial.Serial(
        port, 115200, timeout=0.25, dsrdtr=False, rtscts=False
    ) as device:
        device.reset_input_buffer()
        device.dtr = False
        device.rts = True
        time.sleep(0.1)
        device.rts = False
        deadline = time.monotonic() + 20.0
        while time.monotonic() < deadline:
            captured.extend(device.read(4096))

    output_path = Path("build/hil-diagnostic-boot.log")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(captured)
    text = captured.decode("utf-8", errors="replace")
    assert captured, "reset produced no serial diagnostics"
    assert "ESP-ROM" in text or "rst:" in text, "ESP32 boot banner missing"
    assert "MAIN:" in text, "application startup diagnostics missing"
    lowered = text.lower()
    forbidden = (
        "guru meditation",
        "stack overflow",
        "task watchdog got triggered",
        "assert failed",
        "abort() was called",
        "panic'ed",
    )
    found = [pattern for pattern in forbidden if pattern in lowered]
    assert not found, f"fatal boot diagnostics found: {found}"
