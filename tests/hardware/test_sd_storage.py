"""Physical SD and filesystem checks routed through the public host protocol."""

from __future__ import annotations

import hashlib
import os
import socket
import time
import uuid

import pytest

from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    TcpProtocolClient,
    receive_tcp_frames,
)
from tests.hardware.hil_file_transfer import (
    FILE_COMMAND,
    FILE_COMPLETE,
    FILE_CANCEL,
    FILE_DATA,
    FILE_GEOMETRY,
    FILE_MD5,
    FILE_RETRY,
    FileTransferError,
    download_file,
    upload_file,
)
from tools.wifi_provision_protocol import encode_frame


def _payload(frames) -> bytes:
    """Combines textual target responses for concise command assertions."""

    return b"\n".join(frame.payload for frame in frames)


def _remove(client, path: str) -> None:
    """Removes one test path as best-effort cleanup."""

    try:
        client.exchange(GENERAL_COMMAND, f"rm -R {path}".encode("ascii"), 5.0)
    except Exception:
        # Preserve the original assertion or transport failure if the target
        # disconnected before cleanup could run.
        pass


def _tcp_transfer_exchange(
    connection: socket.socket, frame_type: int, payload: bytes
):
    """Sends one transfer frame on an intentionally persistent TCP socket."""

    connection.sendall(encode_frame(frame_type, payload))
    # Keep a generous response window for slow or briefly unavailable Wi-Fi;
    # continuous silence now reaches inactivity rather than a timed retry.
    return receive_tcp_frames(connection, 7.0)


def _required_frame(frames, frame_type: int, *, allow_prior_cancel: bool = False):
    """Returns one required response and exposes target errors in assertions."""

    requested = next(
        (frame for frame in frames if frame.frame_type == frame_type),
        None,
    )
    if requested is not None and allow_prior_cancel:
        return requested
    errors = [frame.payload for frame in frames if frame.frame_type == FILE_CANCEL]
    assert not errors, errors[0].decode("utf-8", errors="replace")
    return requested


def _await_upload_request(
    connection: socket.socket,
    frames,
    timeout: float = 7.0,
    *,
    expected_sequence: int | None = None,
):
    """Waits for the requested sequence while tolerating retries and stale output."""

    deadline = time.monotonic() + timeout
    collected = list(frames)
    while time.monotonic() < deadline:
        requests = [frame for frame in collected if frame.frame_type == FILE_DATA]
        requested = next(
            (
                frame
                for frame in requests
                if expected_sequence is None
                or int.from_bytes(frame.payload, "big") == expected_sequence
            ),
            None,
        )
        if requested is not None:
            return requested
        has_retry = any(
            frame.frame_type == FILE_RETRY
            and frame.payload == b"Info: need retry!"
            for frame in collected
        )
        # TRN-004 can deliver a request retained for the disconnected slot to
        # its replacement. Keep receiving when such a stale B3 was observed.
        if not has_retry and not requests:
            return None
        collected.clear()
        collected.extend(
            receive_tcp_frames(connection, min(1.0, deadline - time.monotonic()))
        )
    final_request = _required_frame(collected, FILE_DATA)
    if final_request is None or expected_sequence is None:
        return final_request
    return (
        final_request
        if int.from_bytes(final_request.payload, "big") == expected_sequence
        else None
    )


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.sd
@pytest.mark.requirement("SD-001")
@pytest.mark.requirement("FILE-005")
def test_sd_root_can_be_listed(sd_client, sd_fixture) -> None:
    frames = sd_client.exchange(GENERAL_COMMAND, b"ls /sd", timeout_seconds=5.0)
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
    # Keep the generated directory compact so the response remains easy to
    # inspect; current SD-009 permits long names and does not require 8.3.
    path = f"/sd/Z1{uuid.uuid4().hex[:6].upper()}"
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
    source = f"/sd/O{suffix}"
    destination = f"/sd/N{suffix}"
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
        listed = sd_client.exchange(GENERAL_COMMAND, b"ls /sd", 5.0)
        listing = b"".join(frame.payload for frame in listed)
        assert destination.rsplit("/", 1)[1].encode("ascii") + b"/" in listing
        assert source.rsplit("/", 1)[1].encode("ascii") + b"/" not in listing
    finally:
        _remove(sd_client, destination)
        _remove(sd_client, source)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("HFTU-001")
@pytest.mark.requirement("HFTU-003")
@pytest.mark.requirement("HFTU-004")
@pytest.mark.requirement("HFTU-005")
@pytest.mark.requirement("HFTU-007")
@pytest.mark.requirement("HFTD-001")
@pytest.mark.requirement("HFTD-004")
@pytest.mark.requirement("HFTD-005")
@pytest.mark.requirement("HFTD-006")
@pytest.mark.requirement("HFTD-009")
@pytest.mark.requirement("FILE-024")
@pytest.mark.requirement("FILE-025")
@pytest.mark.requirement("FILE-027")
@pytest.mark.requirement("FILE-029")
def test_file_roundtrip_md5_rename_and_delete(usb_client, sd_fixture) -> None:
    """Exercises file transfer and cache lifecycle through real target storage."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    suffix = uuid.uuid4().hex[:5].upper()
    source = f"/sd/F{suffix}.BIN"
    destination = f"/sd/R{suffix}.BIN"
    content = bytes((index * 37 + 11) & 0xFF for index in range(2500))
    digest = hashlib.md5(content).hexdigest().encode("ascii")
    try:
        upload_file(usb_client, source, content, block_size=700)
        md5_frames = usb_client.exchange(
            GENERAL_COMMAND, f"md5sum {source}".encode("ascii"), 5.0
        )
        assert digest in _payload(md5_frames).lower()
        assert download_file(usb_client, source) == content

        renamed = usb_client.exchange(
            GENERAL_COMMAND,
            f"mv {source} {destination}".encode("ascii"),
            5.0,
        )
        assert b"renamed" in _payload(renamed).lower()
        assert download_file(usb_client, destination) == content
    finally:
        _remove(usb_client, destination)
        _remove(usb_client, source)

    with pytest.raises(FileTransferError, match="failed to open file"):
        download_file(usb_client, destination)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.tcp
@pytest.mark.requirement("OWN-008")
@pytest.mark.requirement("TCP-005")
@pytest.mark.requirement("HFTU-005")
def test_large_tcp_upload_resumes_after_connection_loss(
    tcp_host: str, sd_fixture
) -> None:
    """Drops TCP mid-upload and requires the reused slot to request continuation."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    path = f"/sd/N{uuid.uuid4().hex[:5].upper()}.BIN"
    block_size = 4096
    # Keep enough headroom for the mock's bounded diagnostic log while still
    # exercising multiple maximum-sized protocol blocks.
    content = bytes((index * 29 + 7) & 0xFF for index in range(64 * 1024))
    blocks = [
        content[index : index + block_size]
        for index in range(0, len(content), block_size)
    ]
    connection: socket.socket | None = None
    try:
        connection = socket.create_connection((tcp_host, 2222), timeout=4.0)
        # A successful upload start is intentionally silent; MD5 is the first
        # host transfer packet, so submit both before awaiting geometry.
        connection.sendall(
            encode_frame(FILE_COMMAND, f"upload {path}".encode("ascii"))
            + encode_frame(
                FILE_MD5, hashlib.md5(content).hexdigest().encode("ascii")
            )
        )
        assert _required_frame(
            receive_tcp_frames(connection, 4.0),
            FILE_GEOMETRY,
            allow_prior_cancel=True,
        )
        request = _required_frame(
            _tcp_transfer_exchange(
                connection, FILE_GEOMETRY, len(blocks).to_bytes(4, "big")
            ),
            FILE_DATA,
        )
        assert request is not None and int.from_bytes(request.payload, "big") == 1

        interruption_sequence = len(blocks) // 2
        for sequence in range(1, interruption_sequence + 1):
            responses = _tcp_transfer_exchange(
                connection,
                FILE_DATA,
                sequence.to_bytes(4, "big") + blocks[sequence - 1],
            )
            request = _await_upload_request(
                connection, responses, expected_sequence=sequence + 1
            )
            assert request is not None, [
                (frame.frame_type, frame.payload) for frame in responses
            ]

        expected_sequence = interruption_sequence + 1
        assert int.from_bytes(request.payload, "big") == expected_sequence
        connection.shutdown(socket.SHUT_RDWR)
        connection.close()
        connection = None
        # Allow the target's receive worker to release the lowest TCP slot,
        # while remaining well inside the active transfer's timeout window.
        time.sleep(1.0)

        connection = socket.create_connection((tcp_host, 2222), timeout=4.0)
        responses = []
        for sequence in range(expected_sequence, len(blocks) + 1):
            # OWN-008 permits the same logical owner to continue; it does not
            # require a new connection to receive an unsolicited replay of the
            # outstanding request. Resend from the acknowledged boundary.
            responses = _tcp_transfer_exchange(
                connection,
                FILE_DATA,
                sequence.to_bytes(4, "big") + blocks[sequence - 1],
            )
            if sequence < len(blocks):
                request = _await_upload_request(
                    connection, responses, expected_sequence=sequence + 1
                )
                assert request is not None, [
                    (frame.frame_type, frame.payload) for frame in responses
                ]
                assert int.from_bytes(request.payload, "big") == sequence + 1
        assert _required_frame(responses, FILE_COMPLETE) is not None

        connection.shutdown(socket.SHUT_RDWR)
        connection.close()
        connection = None
        digest_frames = TcpProtocolClient(tcp_host).exchange(
            GENERAL_COMMAND, f"md5sum {path}".encode("ascii"), 5.0
        )
        assert hashlib.md5(content).hexdigest().encode("ascii") in _payload(
            digest_frames
        ).lower()
    finally:
        if connection is not None:
            try:
                connection.close()
            except OSError:
                pass
        _remove(TcpProtocolClient(tcp_host), path)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("HFT-010")
@pytest.mark.requirement("HFT-011")
@pytest.mark.requirement("FILE-030")
@pytest.mark.requirement("FILE-031")
def test_gcodes_cache_and_ordinary_sd_names_are_independent(usb_client, sd_fixture) -> None:
    """Checks G-code cache handling without remapping ordinary SD names."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    suffix = uuid.uuid4().hex[:4].upper()
    mapped_name = f"G{suffix}.BIN"
    embedded_name = f"XGCODE{suffix[:2]}.TXT"
    mapped_content = b"mapped-gcodes-token"
    embedded_content = b"ordinary-name-containing-gcode-text"
    try:
        usb_client.exchange(GENERAL_COMMAND, b"mkdir /sd/gcodes", 5.0)
        upload_file(
            usb_client,
            f"/sd/gcodes/{mapped_name}",
            mapped_content,
        )
        assert download_file(usb_client, f"/sd/gcodes/{mapped_name}") == mapped_content

        upload_file(usb_client, f"/sd/{embedded_name}", embedded_content)
        assert download_file(usb_client, f"/sd/{embedded_name}") == embedded_content
    finally:
        _remove(usb_client, f"/sd/gcodes/{mapped_name}")
        _remove(usb_client, f"/sd/{embedded_name}")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("LOG-001")
@pytest.mark.requirement("LOG-006")
def test_serial_log_sentinel_mirrors_diagnostics(usb_client, sd_fixture) -> None:
    """Enables the opt-in mirror and retrieves one generated SD diagnostic."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    try:
        upload_file(usb_client, "/sd/serial.log", b"")
        usb_client.exchange(
            GENERAL_COMMAND, b"md5sum /MISSING.BIN", timeout_seconds=5.0
        )
        time.sleep(0.2)
        log = download_file(usb_client, "/sd/serial.log", verify_md5=False)
        assert b"SD access failed" in log
        assert b"MISSING.BIN" in log
    finally:
        _remove(usb_client, "/sd/serial.log")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("HFTU-010")
@pytest.mark.requirement("FILE-027")
def test_cancelled_upload_removes_partial_file_and_md5_sidecar(
    usb_client, sd_fixture
) -> None:
    """Cancels after one block and requires all partially written artifacts gone."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    path = f"/sd/C{uuid.uuid4().hex[:5].upper()}.BIN"
    content = bytes((index * 17 + 5) & 0xFF for index in range(1024))
    try:
        usb_client.send_encoded(
            encode_frame(FILE_COMMAND, f"upload {path}".encode("ascii"))
            + encode_frame(
                FILE_MD5, hashlib.md5(content).hexdigest().encode("ascii")
            )
        )
        assert _required_frame(usb_client.receive(4.0), FILE_GEOMETRY)
        assert _required_frame(
            usb_client.exchange(FILE_GEOMETRY, (2).to_bytes(4, "big"), 4.0),
            FILE_DATA,
        )
        assert _required_frame(
            usb_client.exchange(
                FILE_DATA, (1).to_bytes(4, "big") + content, 4.0
            ),
            FILE_DATA,
        )

        cancelled = usb_client.exchange(FILE_CANCEL, b"", 4.0)
        cancel = next(
            (frame for frame in cancelled if frame.frame_type == FILE_CANCEL),
            None,
        )
        assert cancel is not None
        assert b"upload canceled" in cancel.payload.lower()
        with pytest.raises(FileTransferError, match="failed to open file"):
            download_file(usb_client, path)
        md5 = usb_client.exchange(
            GENERAL_COMMAND, f"md5sum {path}".encode("ascii"), 5.0
        )
        assert b"file not found" in _payload(md5).lower()
        sidecars = usb_client.exchange(GENERAL_COMMAND, b"ls /.md5", 5.0)
        assert path[1:].encode("ascii") not in _payload(sidecars)
    finally:
        _remove(usb_client, path)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.tcp
@pytest.mark.requirement("OWN-003")
@pytest.mark.requirement("TCP-004")
def test_tcp_filesystem_query_during_usb_upload(
    usb_client, tcp_host: str, sd_fixture
) -> None:
    """Keeps an upload pending while another transport lists the same volume."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    path = f"/sd/X{uuid.uuid4().hex[:5].upper()}.BIN"
    content = bytes((index * 23 + 1) & 0xFF for index in range(2048))
    try:
        usb_client.send_encoded(
            encode_frame(FILE_COMMAND, f"upload {path}".encode("ascii"))
            + encode_frame(
                FILE_MD5, hashlib.md5(content).hexdigest().encode("ascii")
            )
        )
        assert _required_frame(usb_client.receive(4.0), FILE_GEOMETRY)
        assert _required_frame(
            usb_client.exchange(FILE_GEOMETRY, (1).to_bytes(4, "big"), 4.0),
            FILE_DATA,
        )

        listed = TcpProtocolClient(tcp_host).exchange(
            GENERAL_COMMAND, b"ls /sd", timeout_seconds=5.0
        )
        assert listed
        assert b"error" not in _payload(listed).lower()

        completed = usb_client.exchange(
            FILE_DATA, (1).to_bytes(4, "big") + content, 5.0
        )
        assert _required_frame(completed, FILE_COMPLETE)
        assert download_file(usb_client, path) == content
    finally:
        _remove(usb_client, path)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("HFTU-006")
@pytest.mark.requirement("HFTU-010")
def test_full_mock_volume_retries_and_recovers_after_cancel(
    usb_client, sd_fixture
) -> None:
    """Fills only the volatile mock volume, then frees the partial upload."""

    if os.getenv("Z1_HIL_MOCK_SD") != "1":
        pytest.skip("full-volume injection is restricted to Z1_HIL_MOCK_SD=1")
    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    path = f"/sd/V{uuid.uuid4().hex[:5].upper()}.BIN"
    block = bytes((index * 13 + 3) & 0xFF for index in range(8192))
    announced_blocks = 128
    try:
        usb_client.send_encoded(
            encode_frame(FILE_COMMAND, f"upload {path}".encode("ascii"))
            + encode_frame(FILE_MD5, b"0" * 32)
        )
        assert _required_frame(usb_client.receive(4.0), FILE_GEOMETRY)
        assert _required_frame(
            usb_client.exchange(
                FILE_GEOMETRY, announced_blocks.to_bytes(4, "big"), 4.0
            ),
            FILE_DATA,
        )

        write_failure = None
        for sequence in range(1, announced_blocks + 1):
            payload = sequence.to_bytes(4, "big") + block
            requested = None
            for _ in range(4):
                responses = usb_client.exchange(FILE_DATA, payload, 4.0)
                write_failure = next(
                    (
                        frame
                        for frame in responses
                        if frame.frame_type == FILE_RETRY
                        and b"write error" in frame.payload.lower()
                    ),
                    None,
                )
                if write_failure is not None:
                    break
                requested = _required_frame(responses, FILE_DATA)
                if requested is not None:
                    break
                # TRN-005 permits one target response to be omitted. Repeating
                # the same sequence is the normative HFT-022 recovery and the
                # target must not append the duplicate block a second time.
            if write_failure is not None:
                break
            assert requested is not None, "target did not recover the upload prompt"
            assert int.from_bytes(requested.payload, "big") == sequence + 1

        assert write_failure is not None, "mock volume accepted more than 1 MiB"
        cancelled = usb_client.exchange(FILE_CANCEL, b"", 4.0)
        assert any(frame.frame_type == FILE_CANCEL for frame in cancelled), cancelled

        # Successful reuse proves that cancellation released ownership and
        # reclaimed the partial file's FAT allocation.
        probe = b"volume-recovered-after-full"
        upload_file(usb_client, path, probe)
        assert download_file(usb_client, path) == probe
    finally:
        # An assertion must not leave global transfer ownership active for the
        # remainder of the HIL session. Cancellation is harmless after normal
        # completion and deterministically releases any partial upload.
        usb_client.exchange(FILE_CANCEL, b"", 4.0)
        _remove(usb_client, path)
