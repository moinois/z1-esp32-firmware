"""File-transfer error and recovery checks against the volatile mock SD."""

from __future__ import annotations

import hashlib
import os
import socket
import uuid

import pytest

from tests.hardware.hil_file_transfer import (
    FILE_CANCEL,
    FILE_COMMAND,
    FILE_COMPLETE,
    FILE_DATA,
    FILE_GEOMETRY,
    FILE_MD5,
    download_file,
    upload_file,
)
from tests.hardware.hil_protocol import GENERAL_COMMAND, receive_tcp_frames
from tools.wifi_provision_protocol import encode_frame


DEFAULT_DOWNLOAD_MD5 = b"82df799dde08f3d86839e24cb97e74d4"


def _required_frame(frames, expected_type: int):
    """Returns one expected frame and retains observed wire data on failure."""

    frame = next(
        (candidate for candidate in frames if candidate.frame_type == expected_type),
        None,
    )
    assert frame is not None, [
        (candidate.frame_type, candidate.payload) for candidate in frames
    ]
    return frame


def _remove(client, path: str) -> None:
    """Removes one fixture path without masking the primary assertion."""

    try:
        client.exchange(GENERAL_COMMAND, f"rm -R {path}".encode("ascii"), 5.0)
    except Exception:
        pass


def _manual_download(client, path: str) -> tuple[bytes, bytes]:
    """Downloads one file while exposing its advertised MD5 value."""

    advertised = _required_frame(
        client.exchange(
            FILE_COMMAND, f"download {path}".encode("ascii"), 5.0
        ),
        FILE_MD5,
    ).payload
    geometry = _required_frame(
        client.exchange(FILE_GEOMETRY, b"", 5.0), FILE_GEOMETRY
    ).payload
    assert len(geometry) == 6
    count = int.from_bytes(geometry[:4], "big")
    block_size = int.from_bytes(geometry[4:], "big")
    assert count > 0 and block_size > 0

    content = bytearray()
    for sequence in range(1, count + 1):
        packet = _required_frame(
            client.exchange(FILE_DATA, sequence.to_bytes(4, "big"), 5.0),
            FILE_DATA,
        ).payload
        assert len(packet) >= 4
        assert int.from_bytes(packet[:4], "big") == sequence
        assert len(packet[4:]) <= block_size
        content.extend(packet[4:])
    _required_frame(client.exchange(FILE_COMPLETE, b"", 5.0), FILE_COMPLETE)
    return advertised, bytes(content)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.tcp
@pytest.mark.requirement("HFTU-004")
@pytest.mark.requirement("HFTU-005")
@pytest.mark.requirement("HFT-024")
def test_duplicate_upload_packets_repeat_sequence_without_duplicate_bytes(
    usb_client, tcp_host: str, sd_fixture
) -> None:
    """Injects 51 duplicate packets, then completes with exact file bytes."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    path = f"/sd/D{uuid.uuid4().hex[:5].upper()}.BIN"
    first = bytes((index * 7 + 1) & 0xFF for index in range(300))
    second = bytes((index * 11 + 3) & 0xFF for index in range(350))
    content = first + second
    connection: socket.socket | None = None
    try:
        connection = socket.create_connection((tcp_host, 2222), timeout=5.0)
        connection.sendall(
            encode_frame(FILE_COMMAND, f"upload {path}".encode("ascii"))
            + encode_frame(
                FILE_MD5, hashlib.md5(content).hexdigest().encode("ascii")
            )
        )
        _required_frame(receive_tcp_frames(connection, 5.0), FILE_GEOMETRY)
        connection.sendall(encode_frame(FILE_GEOMETRY, (2).to_bytes(4, "big")))
        request = _required_frame(
            receive_tcp_frames(connection, 5.0), FILE_DATA
        )
        assert int.from_bytes(request.payload, "big") == 1

        connection.sendall(
            encode_frame(FILE_DATA, (1).to_bytes(4, "big") + first)
        )
        request = _required_frame(
            receive_tcp_frames(connection, 5.0), FILE_DATA
        )
        assert int.from_bytes(request.payload, "big") == 2

        duplicate = encode_frame(FILE_DATA, (1).to_bytes(4, "big"))
        connection.sendall(duplicate * 51)
        repeated = _required_frame(
            receive_tcp_frames(connection, 5.0), FILE_DATA
        )
        assert int.from_bytes(repeated.payload, "big") == 2

        connection.sendall(
            encode_frame(FILE_DATA, (2).to_bytes(4, "big") + second)
        )
        _required_frame(
            receive_tcp_frames(connection, 5.0), FILE_COMPLETE
        )
        connection.shutdown(socket.SHUT_RDWR)
        connection.close()
        connection = None
        assert download_file(usb_client, path) == content
    finally:
        if connection is not None:
            try:
                connection.sendall(encode_frame(FILE_CANCEL, b""))
            except OSError:
                pass
            connection.close()
        try:
            usb_client.exchange(FILE_CANCEL, b"", 2.0)
        except Exception:
            pass
        _remove(usb_client, path)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("HFTD-010")
@pytest.mark.requirement("HFT-005")
def test_download_cancel_releases_owner_and_preserves_source(
    usb_client, sd_fixture
) -> None:
    """Cancels a download, verifies its source, then starts another transfer."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    suffix = uuid.uuid4().hex[:5].upper()
    source = f"/sd/C{suffix}.BIN"
    successor = f"/sd/N{suffix}.BIN"
    source_content = bytes((index * 19 + 5) & 0xFF for index in range(1400))
    successor_content = b"transfer-after-download-cancel"
    try:
        upload_file(usb_client, source, source_content)
        _required_frame(
            usb_client.exchange(
                FILE_COMMAND, f"download {source}".encode("ascii"), 5.0
            ),
            FILE_MD5,
        )
        cancel = _required_frame(
            usb_client.exchange(FILE_CANCEL, b"", 5.0), FILE_CANCEL
        )
        assert cancel.payload == b"Info: canceled by remote!"

        assert download_file(usb_client, source) == source_content
        upload_file(usb_client, successor, successor_content)
        assert download_file(usb_client, successor) == successor_content
    finally:
        try:
            usb_client.exchange(FILE_CANCEL, b"", 2.0)
        except Exception:
            pass
        _remove(usb_client, source)
        _remove(usb_client, successor)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("HFT-013")
@pytest.mark.requirement("HFTD-005")
def test_missing_and_invalid_md5_sidecars_fall_back_and_recover(
    usb_client, sd_fixture
) -> None:
    """Exercises both absent and malformed cached-MD5 fallback paths."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    name = f"M{uuid.uuid4().hex[:5].upper()}.BIN"
    path = f"/sd/{name}"
    sidecar = f"/sd/.md5/{name}"
    nested_sidecar = f"/sd/.md5/.md5/{name}"
    content = bytes((index * 23 + 7) & 0xFF for index in range(1700))
    try:
        upload_file(usb_client, path, content)
        _remove(usb_client, sidecar)
        advertised, received = _manual_download(usb_client, path)
        assert advertised == DEFAULT_DOWNLOAD_MD5
        assert received == content

        upload_file(usb_client, sidecar, b"not-a-valid-md5")
        advertised, received = _manual_download(usb_client, path)
        assert advertised == DEFAULT_DOWNLOAD_MD5
        assert received == content

        # A new normal upload recreates a valid primary sidecar and restores
        # end-to-end MD5 verification for the same logical path.
        upload_file(usb_client, path, content)
        assert download_file(usb_client, path) == content
    finally:
        try:
            usb_client.exchange(FILE_CANCEL, b"", 2.0)
        except Exception:
            pass
        _remove(usb_client, path)
        _remove(usb_client, sidecar)
        _remove(usb_client, nested_sidecar)
