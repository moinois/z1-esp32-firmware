"""Deterministic target NVS open and commit failure validation."""

from __future__ import annotations

import os
import re
import time

import pytest

from tests.hardware.hil_protocol import GENERAL_COMMAND


_FIRST_BOOT = re.compile(rb"sys-time-data = ([^,]+),")


def _payload(client, command: bytes, timeout: float = 4.0) -> bytes:
    """Returns all payload bytes produced by one framed command."""

    return b"\n".join(
        frame.payload
        for frame in client.exchange(GENERAL_COMMAND, command, timeout)
    )


def _first_boot(client) -> bytes:
    """Returns the persistent first-boot field from a runtime response."""

    payload = _payload(client, b"sys-time")
    match = _FIRST_BOOT.search(payload)
    assert match is not None, payload
    return match.group(1)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.controller
@pytest.mark.requirement("BOOT-001")
@pytest.mark.requirement("BOOT-002")
@pytest.mark.requirement("BOOT-003")
@pytest.mark.requirement("RUN-043")
def test_nvs_open_and_commit_faults_are_visible_and_recoverable(usb_client) -> None:
    """Injects both target NVS boundaries without losing the persisted value."""

    if os.getenv("Z1_HIL_MOCK_NVS") != "1":
        pytest.skip("NVS fault controls require Z1_HIL_MOCK_NVS=1")
    if os.getenv("Z1_HIL_MOCK_CONTROLLER") != "1":
        pytest.skip("first-boot setup requires Z1_HIL_MOCK_CONTROLLER=1")
    assert os.environ["Z1_ALLOW_MUTATION"] == "1"

    try:
        requested_time = int(time.time())
        _payload(
            usb_client,
            f"mock-time {requested_time}".encode("ascii"),
            timeout=1.0,
        )
        deadline = time.monotonic() + 5.0
        before = _first_boot(usb_client)
        while before == b"null" and time.monotonic() < deadline:
            time.sleep(0.1)
            before = _first_boot(usb_client)
        assert before != b"null"

        assert b"fail-open selected" in _payload(
            usb_client, b"mock-nvs fail-open"
        )
        assert b"sn get failed" in _payload(usb_client, b"sn-get").lower()
        assert b"faults cleared" in _payload(usb_client, b"mock-nvs clear")
        assert b"sn get failed" not in _payload(usb_client, b"sn-get").lower()

        assert b"fail-commit selected" in _payload(
            usb_client, b"mock-nvs fail-commit"
        )
        assert b"clearftm failed" in _payload(usb_client, b"clearftm").lower()
        assert b"faults cleared" in _payload(usb_client, b"mock-nvs clear")
        assert _first_boot(usb_client) == before
    finally:
        try:
            _payload(usb_client, b"mock-nvs clear")
        except Exception:
            pass
