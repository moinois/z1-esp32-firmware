"""Latched block-device fault injection for the PSRAM-backed mock SD."""

from __future__ import annotations

import os

import pytest

from tests.hardware.hil_file_transfer import (
    FILE_CANCEL,
    FileTransferError,
    download_file,
    upload_file,
)
from tests.hardware.hil_protocol import GENERAL_COMMAND, TcpProtocolClient


def _payload(frames) -> bytes:
    """Joins framed text independently of transport chunking."""

    return b"\n".join(frame.payload for frame in frames)


def _control(client, action: str) -> bytes:
    """Applies one mock-SD action and returns its exact text response."""

    return _payload(
        client.exchange(
            GENERAL_COMMAND, f"mock-sd {action}".encode("ascii"), 5.0
        )
    )


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.tcp
@pytest.mark.requirement("SD-001")
@pytest.mark.requirement("FILE-010")
@pytest.mark.requirement("FILE-015")
def test_mock_sd_unmount_reports_absence_and_remount_recovers(
    usb_client, tcp_host: str, sd_fixture
) -> None:
    """Unmounts the VFS, rejects access, and recreates a fresh usable volume."""

    if os.getenv("Z1_HIL_MOCK_SD") != "1":
        pytest.skip("mock SD not declared with Z1_HIL_MOCK_SD=1")
    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    tcp = TcpProtocolClient(tcp_host)
    try:
        assert _control(usb_client, "unmount") == b"mock-sd unmounted\n"
        assert _control(tcp, "status") == b"mock-sd unmounted\n"
        # DirectoryListing retains the specified completion frame on open
        # failure; the target adapter now rejects before falling through to a
        # host-directory VFS path and records ENODEV in APP_FILE diagnostics.
        listed = _payload(tcp.exchange(GENERAL_COMMAND, b"ls /", 5.0))
        assert listed == b"Load directory finished.\r\n"
        with pytest.raises(FileTransferError, match="failed to open file"):
            upload_file(usb_client, "/ABSENT.BIN", b"must-not-write")

        assert _control(tcp, "mount") == b"mock-sd mounted\n"
        upload_file(usb_client, "/RECOVER.BIN", b"mounted-again")
        assert download_file(usb_client, "/RECOVER.BIN") == b"mounted-again"
    finally:
        _control(usb_client, "clear")
        if _control(usb_client, "status") == b"mock-sd unmounted\n":
            _control(usb_client, "mount")
        try:
            usb_client.exchange(GENERAL_COMMAND, b"rm /RECOVER.BIN", 4.0)
        except Exception:
            pass


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("SD-002")
@pytest.mark.requirement("HFTU-008")
@pytest.mark.requirement("HFTD-005")
def test_mock_sd_latched_read_write_and_sync_faults_recover(
    usb_client, sd_fixture
) -> None:
    """Forces each disk callback to fail until cleared, then proves reuse."""

    if os.getenv("Z1_HIL_MOCK_SD") != "1":
        pytest.skip("mock SD not declared with Z1_HIL_MOCK_SD=1")
    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    content = bytes((index * 17 + 5) & 0xFF for index in range(16 * 1024))
    try:
        upload_file(usb_client, "/READ.BIN", content)
        assert _control(usb_client, "fail-read") == b"mock-sd fail-read armed\n"
        with pytest.raises(FileTransferError):
            download_file(usb_client, "/READ.BIN")
        assert _control(usb_client, "clear") == b"mock-sd faults cleared\n"
        assert download_file(usb_client, "/READ.BIN") == content

        assert _control(usb_client, "fail-write") == b"mock-sd fail-write armed\n"
        with pytest.raises(FileTransferError):
            upload_file(usb_client, "/WRITE.BIN", b"write-must-fail")
        try:
            usb_client.exchange(FILE_CANCEL, b"", 2.0)
        except Exception:
            pass
        _control(usb_client, "clear")
        # Reformat after a deliberately failed FAT metadata update, then prove
        # that ownership, callbacks, and the VFS all recover together.
        _control(usb_client, "unmount")
        assert _control(usb_client, "mount") == b"mock-sd mounted\n"
        upload_file(usb_client, "/WRITE.BIN", b"write-recovered")
        assert download_file(usb_client, "/WRITE.BIN") == b"write-recovered"

        assert _control(usb_client, "fail-sync") == b"mock-sd fail-sync armed\n"
        failed = _payload(
            usb_client.exchange(GENERAL_COMMAND, b"mkdir /SYNCFAIL", 5.0)
        ).lower()
        assert b"created directory" not in failed
        _control(usb_client, "clear")
        recovered = _payload(
            usb_client.exchange(GENERAL_COMMAND, b"mkdir /SYNCOK", 5.0)
        ).lower()
        assert b"created directory" in recovered
    finally:
        try:
            usb_client.exchange(FILE_CANCEL, b"", 2.0)
        except Exception:
            pass
        _control(usb_client, "clear")
        if _control(usb_client, "status") == b"mock-sd unmounted\n":
            _control(usb_client, "mount")
        for path in ("/READ.BIN", "/WRITE.BIN", "/SYNCFAIL", "/SYNCOK"):
            try:
                usb_client.exchange(
                    GENERAL_COMMAND, f"rm -R {path}".encode("ascii"), 4.0
                )
            except Exception:
                pass
