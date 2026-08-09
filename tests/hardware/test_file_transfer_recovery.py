"""Verifies target file-transfer recovery across temporary network silence."""

from __future__ import annotations

import hashlib
import socket
import time

import pytest

from tests.hardware.hil_file_transfer import (
    FILE_COMMAND,
    FILE_COMPLETE,
    FILE_CANCEL,
    FILE_DATA,
    FILE_GEOMETRY,
    FILE_MD5,
    FILE_RETRY,
    download_file,
    upload_file,
)
from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    TcpProtocolClient,
    receive_tcp_frames,
)
from tools.wifi_provision_protocol import encode_frame


def _expect(
    connection: socket.socket,
    frame_type: int,
    timeout: float = 3.0,
    *,
    allow_prior_cancel: bool = False,
):
    """Returns one expected response frame from the current TCP connection."""

    frames = receive_tcp_frames(connection, timeout)
    assert frames, f"target returned no frame while waiting for 0x{frame_type:02x}"
    if not allow_prior_cancel:
        assert not any(frame.frame_type == 0xB5 for frame in frames), frames
    expected = next(
        (frame for frame in frames if frame.frame_type == frame_type), None
    )
    if expected is None and allow_prior_cancel and any(
        frame.frame_type == 0xB5 for frame in frames
    ):
        # The worker may flush the previous owner's terminal timeout before it
        # emits the response for the newly admitted command on the same slot.
        following = receive_tcp_frames(connection, timeout)
        frames.extend(following)
        expected = next(
            (frame for frame in following if frame.frame_type == frame_type), None
        )
    assert expected is not None, (
        f"expected 0x{frame_type:02x}, received "
        + ", ".join(f"0x{frame.frame_type:02x}" for frame in frames)
    )
    return expected


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.tcp
@pytest.mark.sd
@pytest.mark.requirement("HFT-020")
@pytest.mark.requirement("HFT-021")
@pytest.mark.requirement("HFT-022")
def test_tcp_upload_retries_after_temporary_network_silence(
    tcp_host: str, sd_fixture
) -> None:
    """Continues one upload after a six-second host/network pause."""

    path = "/NETPAUSE.BIN"
    # This case isolates the timing policy; large multi-block recovery is
    # covered separately so Wi-Fi throughput cannot consume the timeout margin.
    content = bytes((index * 19 + 3) & 0xFF for index in range(1024))
    digest = hashlib.md5(content).hexdigest().encode("ascii")
    with socket.create_connection((tcp_host, 2222), timeout=3.0) as connection:
        connection.settimeout(10.0)
        # Successful start is silent; HFTU-003 makes MD5 the first host packet.
        connection.sendall(
            encode_frame(FILE_COMMAND, f"upload {path}".encode())
            + encode_frame(FILE_MD5, digest)
        )
        # A prior interrupted invocation may deliver its terminal timeout when
        # this connection binds the same logical slot. The new B2 response
        # proves that this invocation was nevertheless admitted.
        _expect(connection, FILE_GEOMETRY, allow_prior_cancel=True)
        connection.sendall(encode_frame(FILE_GEOMETRY, (1).to_bytes(4, "big")))
        requested = _expect(connection, FILE_DATA)
        assert requested.payload == (1).to_bytes(4, "big")

        # HFT-022 requires a retry at 5.010 seconds. Six seconds models a short
        # Wi-Fi outage while remaining below HFT-021's nine-second abort limit.
        time.sleep(6.0)
        retry = _expect(connection, FILE_RETRY, timeout=2.0)
        assert retry.payload == b"Info: need retry!"

        connection.sendall(
            encode_frame(FILE_DATA, (1).to_bytes(4, "big") + content)
        )
        _expect(connection, FILE_COMPLETE)

        connection.sendall(encode_frame(GENERAL_COMMAND, f"rm {path}".encode()))
        _expect(connection, 0x84)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.tcp
@pytest.mark.usb
@pytest.mark.sd
@pytest.mark.requirement("HFT-020")
@pytest.mark.requirement("HFT-021")
@pytest.mark.requirement("HFT-023")
@pytest.mark.requirement("HFT-025")
def test_tcp_download_inactivity_aborts_and_releases_owner(
    usb_client, tcp_host: str, sd_fixture
) -> None:
    """Observes the target timer abort, source preservation, and reuse."""

    path = "/DLPAUSE.BIN"
    successor = "/DLNEXT.BIN"
    content = bytes((index * 29 + 11) & 0xFF for index in range(1300))
    next_content = b"transfer-after-download-timeout"
    try:
        # A preceding TCP owner is released by the target's worker poll after
        # its connection closes. Wait without issuing competing commands so
        # this case starts from an observable idle-owner boundary.
        time.sleep(10.5)
        upload_file(usb_client, path, content)
        with socket.create_connection((tcp_host, 2222), timeout=3.0) as connection:
            connection.settimeout(12.0)
            connection.sendall(
                encode_frame(FILE_COMMAND, f"download {path}".encode("ascii"))
            )
            _expect(connection, FILE_MD5)

            # HFT-021 aborts after more than nine seconds without another
            # download packet. Waiting ten seconds exercises the target's ESP
            # timer polling rather than a host-side synthetic timeout.
            time.sleep(10.0)
            cancelled = _expect(connection, 0xB5, timeout=2.0, allow_prior_cancel=True)
            assert cancelled.payload == b"Error: Machine received cmd timeout!"

        assert download_file(usb_client, path) == content
        upload_file(usb_client, successor, next_content)
        assert download_file(usb_client, successor) == next_content
    finally:
        for fixture_path in (path, successor):
            try:
                usb_client.exchange(
                    GENERAL_COMMAND, f"rm {fixture_path}".encode("ascii"), 4.0
                )
            except Exception:
                pass


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.tcp
@pytest.mark.usb
@pytest.mark.sd
@pytest.mark.requirement("HFT-023")
@pytest.mark.requirement("HFT-025")
def test_tcp_download_protocol_errors_preserve_source_and_recover(
    usb_client, tcp_host: str, sd_fixture
) -> None:
    """Injects excessive commands and sequence zero through production TCP."""

    path = "/DLERROR.BIN"
    content = bytes((index * 31 + 13) & 0xFF for index in range(1150))

    def start_download(connection: socket.socket) -> None:
        """Starts one download and consumes its initial MD5 response."""

        connection.sendall(
            encode_frame(FILE_COMMAND, f"download {path}".encode("ascii"))
        )
        _expect(connection, FILE_MD5)

    try:
        # Release a resumable transfer left by an interrupted prior invocation.
        # A no-op cancel is harmless when the slot has no retained owner.
        TcpProtocolClient(tcp_host).exchange(FILE_CANCEL, b"", 2.0)
        upload_file(usb_client, path, content)

        with socket.create_connection((tcp_host, 2222), timeout=3.0) as connection:
            start_download(connection)
            connection.sendall(encode_frame(FILE_DATA, b"") * 51)
            cancelled = _expect(
                connection, FILE_CANCEL, timeout=3.0, allow_prior_cancel=True
            )
            assert cancelled.payload == b"Error: Machine received too many wrong command!"

        with socket.create_connection((tcp_host, 2222), timeout=3.0) as connection:
            start_download(connection)
            connection.sendall(encode_frame(FILE_DATA, (0).to_bytes(4, "big")))
            cancelled = _expect(
                connection, FILE_CANCEL, timeout=3.0, allow_prior_cancel=True
            )
            assert cancelled.payload == b"Error: Machine received cmd timeout!"

        assert download_file(usb_client, path) == content
    finally:
        try:
            TcpProtocolClient(tcp_host).exchange(FILE_CANCEL, b"", 2.0)
        except Exception:
            pass
        try:
            usb_client.exchange(
                GENERAL_COMMAND, f"rm {path}".encode("ascii"), 4.0
            )
        except Exception:
            pass
