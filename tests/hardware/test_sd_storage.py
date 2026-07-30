"""Physical SD and filesystem checks routed through the public host protocol."""

from __future__ import annotations

import os
import uuid

import pytest

from tests.hardware.hil_protocol import GENERAL_COMMAND


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.sd
@pytest.mark.requirement("SD-001")
@pytest.mark.requirement("FILE-005")
def test_sd_root_can_be_listed(sd_client, sd_fixture) -> None:
    frames = sd_client.exchange(GENERAL_COMMAND, b"ls /", timeout_seconds=5.0)
    assert frames, "no response; SD card may be absent or unmounted"
    combined = b"\n".join(frame.payload for frame in frames).lower()
    assert b"error" not in combined, combined.decode("utf-8", errors="replace")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.requirement("FILE-020")
@pytest.mark.requirement("FILE-021")
@pytest.mark.requirement("FILE-005")
def test_temporary_directory_create_and_remove(sd_client, sd_fixture) -> None:
    """Creates and removes a unique directory after explicit mutation opt-in."""
    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    # The product uses FAT short-name mode; keep the unique directory component
    # within the eight-character 8.3 base-name limit.
    path = f"/Z1{uuid.uuid4().hex[:6].upper()}"
    try:
        created = sd_client.exchange(
            GENERAL_COMMAND, f"mkdir {path}".encode("ascii"), timeout_seconds=5.0
        )
        assert created
        payload = b"\n".join(frame.payload for frame in created).lower()
        assert b"created directory" in payload, payload.decode(errors="replace")
        assert b"created directory /" in payload, payload.decode(errors="replace")
    finally:
        removed = sd_client.exchange(
            GENERAL_COMMAND, f"rm -R {path}".encode("ascii"), timeout_seconds=5.0
        )
    assert removed
    payload = b"\n".join(frame.payload for frame in removed).lower()
    assert b"could not delete" not in payload, payload.decode(errors="replace")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.requirement("FILE-015")
@pytest.mark.requirement("FILE-024")
@pytest.mark.requirement("FILE-025")
def test_directory_rename_is_visible_in_root_listing(sd_client, sd_fixture) -> None:
    """Exercises real FAT rename and enumeration through the public protocol."""
    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    suffix = uuid.uuid4().hex[:5].upper()
    source = f"/O{suffix}"
    destination = f"/N{suffix}"
    try:
        created = sd_client.exchange(
            GENERAL_COMMAND, f"mkdir {source}".encode("ascii"), 5.0
        )
        assert any(b"created directory" in frame.payload.lower() for frame in created)
        renamed = sd_client.exchange(
            GENERAL_COMMAND,
            f"mv {source} {destination}".encode("ascii"),
            5.0,
        )
        assert any(b"renamed" in frame.payload.lower() for frame in renamed)
        listed = sd_client.exchange(GENERAL_COMMAND, b"ls /", 5.0)
        listing = b"".join(frame.payload for frame in listed)
        assert destination[1:].encode("ascii") + b"/" in listing
        assert source[1:].encode("ascii") + b"/" not in listing
    finally:
        sd_client.exchange(
            GENERAL_COMMAND, f"rm -R {destination}".encode("ascii"), 5.0
        )
        sd_client.exchange(
            GENERAL_COMMAND, f"rm -R {source}".encode("ascii"), 5.0
        )


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.requirement("HFT-004")
@pytest.mark.requirement("FILE-020")
def test_parent_traversal_remains_sandboxed_to_sd_root(sd_client, sd_fixture) -> None:
    """Proves traversal is normalized to the user-visible SD root on real VFS calls."""
    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    name = f"S{uuid.uuid4().hex[:6].upper()}"
    try:
        created = sd_client.exchange(
            GENERAL_COMMAND,
            f"mkdir /../../{name}".encode("ascii"),
            5.0,
        )
        message = b"".join(frame.payload for frame in created)
        assert f"created directory /{name}".encode("ascii") in message
        listed = sd_client.exchange(GENERAL_COMMAND, b"ls /", 5.0)
        assert name.encode("ascii") + b"/" in b"".join(
            frame.payload for frame in listed
        )
    finally:
        sd_client.exchange(
            GENERAL_COMMAND, f"rm -R /{name}".encode("ascii"), 5.0
        )
