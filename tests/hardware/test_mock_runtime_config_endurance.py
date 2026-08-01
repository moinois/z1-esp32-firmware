"""Runtime, configuration concurrency, and storage endurance on mock hardware."""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import os
import re
import time
import uuid

import pytest

from tests.hardware.hil_file_transfer import download_file, upload_file
from tests.hardware.hil_protocol import GENERAL_COMMAND, TcpProtocolClient


_RUNTIME_PATTERN = re.compile(rb"sys-time-data = ([^,]+),(\d+),(\d+)")


def _payload(frames) -> bytes:
    """Joins response payloads without relying on transport packet boundaries."""

    return b"\n".join(frame.payload for frame in frames)


def _runtime(client) -> tuple[bytes, int, int]:
    """Returns first boot, power-on seconds, and machine seconds."""

    payload = _payload(client.exchange(GENERAL_COMMAND, b"sys-time", 5.0))
    match = _RUNTIME_PATTERN.search(payload)
    assert match is not None, payload
    return match.group(1), int(match.group(2)), int(match.group(3))


def _remove(client, path: str) -> None:
    """Removes a fixture path without hiding the primary assertion."""

    try:
        client.exchange(GENERAL_COMMAND, f"rm -R {path}".encode("ascii"), 5.0)
    except Exception:
        pass


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.tcp
@pytest.mark.controller
@pytest.mark.requirement("RUN-010")
@pytest.mark.requirement("RUN-030")
@pytest.mark.requirement("RUN-031")
@pytest.mark.requirement("RUN-040")
@pytest.mark.requirement("RUN-043")
def test_runtime_mutation_is_shared_across_usb_tcp_and_controller(
    usb_client, tcp_host: str
) -> None:
    """Clears via TCP, recreates via controller, and reads through both hosts."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    if os.getenv("Z1_HIL_MOCK_CONTROLLER") != "1":
        pytest.skip("controller-origin time injection requires the controller mock")
    tcp = TcpProtocolClient(tcp_host)
    serial_usb = _payload(usb_client.exchange(GENERAL_COMMAND, b"sn-get", 5.0))
    serial_tcp = _payload(tcp.exchange(GENERAL_COMMAND, b"sn-get", 5.0))
    assert serial_usb == serial_tcp

    cleared = _payload(tcp.exchange(GENERAL_COMMAND, b"clearftm", 5.0)).lower()
    assert b"clearftm ok" in cleared
    assert _runtime(usb_client)[0] == b"null"

    requested_time = int(time.time())
    usb_client.exchange(
        GENERAL_COMMAND, f"mock-time {requested_time}".encode("ascii"), 2.0
    )
    deadline = time.monotonic() + 5.0
    usb_runtime = _runtime(usb_client)
    while usb_runtime[0] == b"null" and time.monotonic() < deadline:
        time.sleep(0.1)
        usb_runtime = _runtime(usb_client)
    assert usb_runtime[0] != b"null"
    assert _runtime(tcp)[0] == usb_runtime[0]


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.tcp
@pytest.mark.requirement("CFG-010")
@pytest.mark.requirement("CFG-030")
@pytest.mark.requirement("CFG-031")
@pytest.mark.requirement("CFG-034")
def test_configuration_errors_and_concurrent_updates_preserve_document(
    usb_client, tcp_host: str, sd_fixture
) -> None:
    """Rejects bad syntax and retains both simultaneous transport updates."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    tcp = TcpProtocolClient(tcp_host)
    initial = b"MAINBOARD_USBA=before\nMAINBOARD_TCPB=before\n"
    try:
        upload_file(usb_client, "/config.txt", initial)
        malformed = _payload(
            tcp.exchange(GENERAL_COMMAND, b"config-set sd ONLYKEY", 5.0)
        )
        assert malformed == (
            b"Usage: config-set source setting value # where source is sd, "
            b"setting is the key and value is the new value\r\n"
        )
        assert download_file(usb_client, "/config.txt") == initial

        with ThreadPoolExecutor(max_workers=2) as executor:
            usb_result = executor.submit(
                usb_client.exchange,
                GENERAL_COMMAND,
                b"config-set sd USBA usb-value",
                5.0,
            )
            tcp_result = executor.submit(
                tcp.exchange,
                GENERAL_COMMAND,
                b"config-set sd TCPB tcp-value",
                5.0,
            )
            assert b"has been set" in _payload(usb_result.result())
            assert b"has been set" in _payload(tcp_result.result())

        content = download_file(usb_client, "/config.txt")
        assert b"MAINBOARD_USBA=usb-value" in content
        assert b"MAINBOARD_TCPB=tcp-value" in content
        assert b"MAINBOARD_USBA=" in content and b"MAINBOARD_TCPB=" in content
    finally:
        _remove(usb_client, "/config.txt")
        _remove(usb_client, "/config.tmp")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.sd
@pytest.mark.usb
@pytest.mark.requirement("HFTU-001")
@pytest.mark.requirement("HFTD-001")
@pytest.mark.requirement("FILE-020")
@pytest.mark.requirement("FILE-023")
def test_mock_sd_repeated_transfer_and_metadata_cycles(usb_client, sd_fixture) -> None:
    """Repeatedly uploads, verifies, renames, and deletes varied FAT files."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    directory = f"/E{uuid.uuid4().hex[:5].upper()}"
    try:
        created = _payload(
            usb_client.exchange(
                GENERAL_COMMAND, f"mkdir {directory}".encode("ascii"), 5.0
            )
        ).lower()
        assert b"created directory" in created
        for cycle in range(20):
            source = f"{directory}/S{cycle:02d}.BIN"
            moved = f"{directory}/M{cycle:02d}.BIN"
            size = 1024 + cycle * 173
            content = bytes((index * (cycle + 3) + cycle) & 0xFF for index in range(size))
            upload_file(usb_client, source, content, block_size=511)
            assert download_file(usb_client, source) == content
            renamed = _payload(
                usb_client.exchange(
                    GENERAL_COMMAND,
                    f"mv {source} {moved}".encode("ascii"),
                    5.0,
                )
            ).lower()
            assert b"renamed" in renamed
            assert download_file(usb_client, moved) == content
            removed = _payload(
                usb_client.exchange(
                    GENERAL_COMMAND, f"rm {moved}".encode("ascii"), 5.0
                )
            ).lower()
            assert b"ok" in removed
    finally:
        _remove(usb_client, directory)
