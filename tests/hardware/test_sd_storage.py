"""Physical SD and filesystem checks routed through the public host protocol."""

from __future__ import annotations

import os
import uuid

import pytest

from tests.hardware.hil_protocol import GENERAL_COMMAND


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.usb
@pytest.mark.sd
@pytest.mark.requirement("SD-001")
@pytest.mark.requirement("FILE-005")
def test_sd_root_can_be_listed_over_usb(usb_client, sd_fixture) -> None:
    frames = usb_client.exchange(GENERAL_COMMAND, b"ls /", timeout_seconds=5.0)
    assert frames, "no response; SD card may be absent or unmounted"
    combined = b"\n".join(frame.payload for frame in frames).lower()
    assert b"error" not in combined, combined.decode("utf-8", errors="replace")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.sd
@pytest.mark.requirement("FILE-020")
@pytest.mark.requirement("FILE-021")
@pytest.mark.requirement("FILE-005")
def test_temporary_directory_create_and_remove(usb_client, sd_fixture) -> None:
    """Creates and removes a unique directory after explicit mutation opt-in."""
    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    # The product uses FAT short-name mode; keep the unique directory component
    # within the eight-character 8.3 base-name limit.
    path = f"/Z1{uuid.uuid4().hex[:6].upper()}"
    try:
        created = usb_client.exchange(
            GENERAL_COMMAND, f"mkdir {path}".encode("ascii"), timeout_seconds=5.0
        )
        assert created
        payload = b"\n".join(frame.payload for frame in created).lower()
        assert b"created directory" in payload, payload.decode(errors="replace")
        assert b"created directory /" in payload, payload.decode(errors="replace")
    finally:
        removed = usb_client.exchange(
            GENERAL_COMMAND, f"rm -R {path}".encode("ascii"), timeout_seconds=5.0
        )
    assert removed
    payload = b"\n".join(frame.payload for frame in removed).lower()
    assert b"could not delete" not in payload, payload.decode(errors="replace")
