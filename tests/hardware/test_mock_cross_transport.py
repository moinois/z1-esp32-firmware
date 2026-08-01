"""Cross-transport configuration and storage checks against mock SD."""

from __future__ import annotations

import hashlib
import os
import socket
import time
import uuid

import pytest

from tests.hardware.hil_file_transfer import (
    FILE_CANCEL,
    FILE_COMMAND,
    FILE_COMPLETE,
    FILE_DATA,
    FILE_GEOMETRY,
    FILE_MD5,
    FileTransferError,
    download_file,
    upload_file,
)
from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    TcpProtocolClient,
    receive_tcp_frames,
)
from tools.wifi_provision_protocol import encode_frame


def _payload(frames) -> bytes:
    """Joins response payloads without depending on transport chunking."""

    return b"\n".join(frame.payload for frame in frames)


def _remove(client, path: str) -> None:
    """Best-effort cleanup for mutation-gated fixture paths."""

    try:
        client.exchange(GENERAL_COMMAND, f"rm -R {path}".encode("ascii"), 5.0)
    except Exception:
        pass


def _required_frame(frames, expected_type: int):
    """Returns one required frame with useful wire evidence on failure."""

    frame = next(
        (candidate for candidate in frames if candidate.frame_type == expected_type),
        None,
    )
    assert frame is not None, [
        (candidate.frame_type, candidate.payload) for candidate in frames
    ]
    return frame


def _upload_over_persistent_tcp(host: str, path: str, content: bytes) -> None:
    """Completes a TCP upload whose silent start and MD5 share one socket."""

    block_size = 700
    blocks = [
        content[offset : offset + block_size]
        for offset in range(0, len(content), block_size)
    ] or [b""]
    with socket.create_connection((host, 2222), timeout=5.0) as connection:
        connection.sendall(
            encode_frame(FILE_COMMAND, f"upload {path}".encode("ascii"))
            + encode_frame(
                FILE_MD5, hashlib.md5(content).hexdigest().encode("ascii")
            )
        )
        _required_frame(receive_tcp_frames(connection, 5.0), FILE_GEOMETRY)
        connection.sendall(
            encode_frame(FILE_GEOMETRY, len(blocks).to_bytes(4, "big"))
        )
        request = _required_frame(receive_tcp_frames(connection, 5.0), FILE_DATA)
        assert int.from_bytes(request.payload, "big") == 1

        for sequence, block in enumerate(blocks, start=1):
            connection.sendall(
                encode_frame(
                    FILE_DATA, sequence.to_bytes(4, "big") + block
                )
            )
            expected = FILE_DATA if sequence < len(blocks) else FILE_COMPLETE
            response = _required_frame(
                receive_tcp_frames(connection, 5.0), expected
            )
            if expected == FILE_DATA:
                assert int.from_bytes(response.payload, "big") == sequence + 1


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.tcp
@pytest.mark.requirement("CFG-010")
@pytest.mark.requirement("CFG-011")
@pytest.mark.requirement("CFG-012")
@pytest.mark.requirement("CFG-030")
def test_configuration_get_and_set_are_shared_between_usb_and_tcp(
    usb_client, tcp_host: str, sd_fixture
) -> None:
    """Mutates and reads one persistent setting through both transports."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    tcp = TcpProtocolClient(tcp_host)
    initial = b"# cross-transport HIL\nMAINBOARD_HILVALUE=before\n"
    try:
        upload_file(usb_client, "/config.txt", initial)

        usb_read = usb_client.exchange(
            GENERAL_COMMAND, b"config-get sd HILVALUE", 5.0
        )
        assert b"sd: HILVALUE is set to before" in _payload(usb_read)

        tcp_set = tcp.exchange(
            GENERAL_COMMAND, b"config-set sd HILVALUE from-tcp", 5.0
        )
        assert b"sd: HILVALUE has been set to from-tcp" in _payload(tcp_set)
        assert b"MAINBOARD_HILVALUE=from-tcp" in download_file(
            usb_client, "/config.txt"
        )

        usb_set = usb_client.exchange(
            GENERAL_COMMAND, b"config-set sd HILVALUE from-usb", 5.0
        )
        assert b"sd: HILVALUE has been set to from-usb" in _payload(usb_set)
        tcp_read = tcp.exchange(
            GENERAL_COMMAND, b"config-get sd HILVALUE", 5.0
        )
        assert b"sd: HILVALUE is set to from-usb" in _payload(tcp_read)

        invalid = tcp.exchange(
            GENERAL_COMMAND, b"config-set cloud HILVALUE rejected", 5.0
        )
        assert b"cloud source does not exist" in _payload(invalid)
        assert b"MAINBOARD_HILVALUE=from-usb" in download_file(
            usb_client, "/config.txt"
        )
    finally:
        _remove(usb_client, "/config.default")
        _remove(usb_client, "/config.txt")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.tcp
@pytest.mark.requirement("SD-009")
@pytest.mark.requirement("CFG-001")
@pytest.mark.requirement("CFG-004")
@pytest.mark.requirement("CFG-005")
@pytest.mark.requirement("CFG-006")
def test_configuration_default_and_restore_expose_83_filename_conflict(
    usb_client, tcp_host: str, sd_fixture
) -> None:
    """Runs default/restore while retaining the normative 8.3 conflict."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    initial = b"MAINBOARD_HILVALUE=snapshot\n"
    try:
        upload_file(usb_client, "/config.txt", initial)
        saved = usb_client.exchange(GENERAL_COMMAND, b"config-default", 5.0)
        payload = _payload(saved)
        if b"Default file not found or created fail" in payload:
            pytest.xfail(
                "SD-009 disables long filenames but CFG-001 requires "
                "/sd/config.default, whose extension is not 8.3-compatible"
            )
        assert b"Settings save as default complete." in payload

        usb_client.exchange(
            GENERAL_COMMAND, b"config-set sd HILVALUE changed", 5.0
        )
        restored = TcpProtocolClient(tcp_host).exchange(
            GENERAL_COMMAND, b"config-restore", 5.0
        )
        assert b"Settings restored complete." in _payload(restored)
        assert download_file(usb_client, "/config.txt") == initial
    finally:
        _remove(usb_client, "/config.default")
        _remove(usb_client, "/config.txt")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.tcp
@pytest.mark.requirement("FILE-011")
@pytest.mark.requirement("FILE-020")
@pytest.mark.requirement("FILE-023")
@pytest.mark.requirement("FILE-024")
@pytest.mark.requirement("FILE-026")
def test_tcp_filesystem_mutations_are_visible_over_usb(
    usb_client, tcp_host: str, sd_fixture
) -> None:
    """Creates and renames through TCP while USB verifies actual FAT state."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    tcp = TcpProtocolClient(tcp_host)
    suffix = uuid.uuid4().hex[:5].upper()
    directory = f"/T{suffix}"
    source = f"{directory}/SOURCE.BIN"
    destination = f"{directory}/MOVED.BIN"
    content = b"cross-transport-filesystem-state"
    try:
        created = tcp.exchange(
            GENERAL_COMMAND, f"mkdir {directory}".encode("ascii"), 5.0
        )
        assert b"created directory" in _payload(created).lower()
        listed = usb_client.exchange(GENERAL_COMMAND, b"ls /", 5.0)
        assert directory[1:].encode("ascii") in _payload(listed)

        upload_file(usb_client, source, content)
        renamed = tcp.exchange(
            GENERAL_COMMAND,
            f"mv {source} {destination}".encode("ascii"),
            5.0,
        )
        assert b"renamed" in _payload(renamed).lower()
        assert download_file(usb_client, destination) == content

        removed = tcp.exchange(
            GENERAL_COMMAND, f"rm {destination}".encode("ascii"), 5.0
        )
        assert b"ok" in _payload(removed).lower()
        with pytest.raises(FileTransferError, match="failed to open file"):
            download_file(usb_client, destination)
    finally:
        _remove(usb_client, directory)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.tcp
@pytest.mark.requirement("HFT-005")
@pytest.mark.requirement("HFT-007")
@pytest.mark.requirement("OWN-001")
@pytest.mark.requirement("OWN-003")
def test_usb_upload_ownership_rejects_tcp_then_recovers(
    usb_client, tcp_host: str, sd_fixture
) -> None:
    """Keeps USB active, rejects a TCP transfer, then proves owner release."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    suffix = uuid.uuid4().hex[:5].upper()
    usb_path = f"/U{suffix}.BIN"
    tcp_path = f"/T{suffix}.BIN"
    usb_content = bytes((index * 7 + 3) & 0xFF for index in range(900))
    tcp_content = bytes((index * 11 + 5) & 0xFF for index in range(1200))
    tcp = TcpProtocolClient(tcp_host)
    try:
        # A previous interrupted TCP fixture can intentionally retain its
        # logical slot and transfer. Rebind the lowest slot and cancel it so
        # this ownership test always begins from a known idle state.
        try:
            tcp.exchange(FILE_CANCEL, b"", 2.0)
        except Exception:
            pass
        time.sleep(0.5)
        usb_client.send_encoded(
            encode_frame(FILE_COMMAND, f"upload {usb_path}".encode("ascii"))
            + encode_frame(
                FILE_MD5, hashlib.md5(usb_content).hexdigest().encode("ascii")
            )
        )
        geometry = usb_client.receive(4.0)
        assert any(frame.frame_type == FILE_GEOMETRY for frame in geometry), [
            (frame.frame_type, frame.payload) for frame in geometry
        ]
        requested = usb_client.exchange(
            FILE_GEOMETRY, (2).to_bytes(4, "big"), 4.0
        )
        assert any(frame.frame_type == FILE_DATA for frame in requested)

        rejected = tcp.exchange(
            FILE_COMMAND, f"upload {tcp_path}".encode("ascii"), 4.0
        )
        cancel = next(
            (frame for frame in rejected if frame.frame_type == FILE_CANCEL), None
        )
        assert cancel is not None
        assert b"Other client is currently uploading/downloading files" in cancel.payload

        cancelled = usb_client.exchange(FILE_CANCEL, b"", 4.0)
        assert any(frame.frame_type == FILE_CANCEL for frame in cancelled)

        _upload_over_persistent_tcp(tcp_host, tcp_path, tcp_content)
        assert download_file(usb_client, tcp_path) == tcp_content
    finally:
        try:
            usb_client.exchange(FILE_CANCEL, b"", 2.0)
        except Exception:
            pass
        _remove(usb_client, usb_path)
        _remove(usb_client, tcp_path)
