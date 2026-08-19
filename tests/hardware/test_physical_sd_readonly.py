"""Read-only checks against the SD card physically installed in a Makera Z1."""

from __future__ import annotations

import hashlib

import pytest

from tests.hardware.hil_file_transfer import download_file
from tests.hardware.hil_protocol import GENERAL_COMMAND, TcpProtocolClient


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.sd
@pytest.mark.requirement("SD-001")
@pytest.mark.requirement("CFG-001")
@pytest.mark.requirement("HFTD-001")
def test_physical_config_is_listed_and_downloads_with_valid_md5(
    sd_client, sd_fixture
) -> None:
    """Uses the factory configuration as an existing non-mutating SD fixture."""

    # The specification addresses the installed card through its explicit
    # `/sd` VFS path; generic HFT-004 normalization is host-tested separately.
    listing = sd_client.exchange(GENERAL_COMMAND, b"ls /sd", 5.0)
    combined = b"\n".join(frame.payload for frame in listing)
    assert b"config.txt" in combined.lower(), combined

    content = download_file(sd_client, "/sd/config.txt", timeout_seconds=8.0)
    assert content.startswith(b"### Carvera settings")
    assert b"sd_ok true" in content
    assert hashlib.md5(content).digest() != bytes(16)


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.sd
@pytest.mark.tcp
@pytest.mark.usb
@pytest.mark.requirement("HFTD-001")
def test_physical_config_is_identical_over_usb_and_tcp(
    usb_client, tcp_host: str, sd_fixture
) -> None:
    """Compares both production transport paths without modifying the card."""

    usb_content = download_file(usb_client, "/sd/config.txt", timeout_seconds=8.0)
    tcp_content = download_file(
        TcpProtocolClient(tcp_host), "/sd/config.txt", timeout_seconds=8.0
    )
    assert tcp_content == usb_content
