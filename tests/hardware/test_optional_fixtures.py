"""Capability gates for fixture-dependent physical validation domains."""

from __future__ import annotations

import os

import pytest


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.can
@pytest.mark.controller
@pytest.mark.requirement("LPC-001")
@pytest.mark.requirement("UART-003")
def test_controller_fixture_declares_connection() -> None:
    if os.getenv("Z1_HIL_CONTROLLER") != "1":
        pytest.skip("external controller fixture not declared with Z1_HIL_CONTROLLER=1")
    pytest.skip("controller fixture driver is not implemented yet")


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.can
@pytest.mark.requirement("CAN-001")
def test_can_fixture_declares_connection() -> None:
    if os.getenv("Z1_HIL_CAN") != "1":
        pytest.skip("CAN fixture not declared with Z1_HIL_CAN=1")
    pytest.skip("CAN fixture driver is not implemented yet")


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.camera
@pytest.mark.requirement("HW-040")
def test_camera_fixture_detects_sensor(camera_fixture) -> None:
    """Records camera availability without failing camera-less mainboards."""
    assert camera_fixture is None
