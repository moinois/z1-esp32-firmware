"""Read-only communication checks for a physically attached motion controller."""

from __future__ import annotations

import os
import re

import pytest

from tests.hardware.hil_protocol import GENERAL_COMMAND, SINGLE_COMMAND


def _require_physical_controller() -> None:
    """Skips unless the operator declares the installed controller board."""

    if os.getenv("Z1_HIL_CONTROLLER") != "1":
        pytest.skip("physical controller not declared with Z1_HIL_CONTROLLER=1")


def _payload_for(frames, frame_type: int) -> bytes:
    """Returns the first required controller response payload."""

    frame = next((item for item in frames if item.frame_type == frame_type), None)
    assert frame is not None, [(hex(item.frame_type), item.payload) for item in frames]
    return frame.payload


def _version_payload(usb_client) -> bytes:
    """Ignores retained console output until the controller version arrives."""

    observed = []
    for _ in range(2):
        observed.extend(usb_client.exchange(GENERAL_COMMAND, b"version", 5.0))
        version = next(
            (
                item.payload
                for item in observed
                if item.frame_type == 0x90
                and re.fullmatch(rb"version = \d+(?:\.\d+)+\n", item.payload)
            ),
            None,
        )
        if version is not None:
            return version
    pytest.fail(
        "no controller version response: "
        + repr([(hex(item.frame_type), item.payload) for item in observed])
    )


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.usb
@pytest.mark.controller
@pytest.mark.requirement("LPC-001")
@pytest.mark.requirement("UART-003")
def test_physical_controller_reports_version_status_and_diagnostics(usb_client) -> None:
    """Proves bidirectional UART composition without issuing motion commands."""

    _require_physical_controller()
    version = _version_payload(usb_client)
    assert re.fullmatch(rb"version = \d+(?:\.\d+)+\n", version), version

    status = _payload_for(usb_client.exchange(SINGLE_COMMAND, b"?", 5.0), 0x81)
    assert status.startswith(b"<Idle|")
    assert b"MPos:" in status and b"WPos:" in status
    assert status.endswith(b">\n")

    diagnostic = _payload_for(
        usb_client.exchange(GENERAL_COMMAND, b"diagnose", 5.0), 0x82
    )
    assert b"|C:1|" in diagnostic, diagnostic
    assert b"|RSSI:" in diagnostic


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.usb
@pytest.mark.controller
@pytest.mark.requirement("UART-003")
def test_physical_controller_remains_responsive_across_repeated_queries(usb_client) -> None:
    """Detects stalled routing or leaked response ownership across safe reads."""

    _require_physical_controller()
    for _ in range(10):
        status = _payload_for(usb_client.exchange(SINGLE_COMMAND, b"?", 5.0), 0x81)
        assert b"MPos:" in status
