"""Verifies target file-transfer recovery across temporary network silence."""

from __future__ import annotations

import hashlib
import socket
import time

import pytest

from tests.hardware.hil_file_transfer import (
    FILE_COMMAND,
    FILE_COMPLETE,
    FILE_DATA,
    FILE_GEOMETRY,
    FILE_MD5,
    FILE_RETRY,
)
from tests.hardware.hil_protocol import GENERAL_COMMAND, receive_tcp_frames
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
