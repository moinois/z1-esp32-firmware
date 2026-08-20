"""Read-only runtime, Wi-Fi, and cross-transport target validation."""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import json
import signal
import socket
import time
import urllib.request

import pytest

from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    SINGLE_COMMAND,
    TcpProtocolClient,
)


def _payloads(frames) -> bytes:
    return b"\n".join(frame.payload for frame in frames)


def _directory_snapshot(client, path: bytes) -> bytes:
    """Returns directory data without depending on protocol frame chunking."""

    return b"".join(
        frame.payload
        for frame in client.exchange(GENERAL_COMMAND, b"ls " + path, 5.0)
        if frame.frame_type == 0x83
    )


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.usb
@pytest.mark.requirement("RUN-010")
@pytest.mark.requirement("RUN-040")
def test_usb_runtime_and_serial_reads(usb_client) -> None:
    assert b"sys-time-data" in _payloads(
        usb_client.exchange(GENERAL_COMMAND, b"sys-time", 4.0)
    )
    assert usb_client.exchange(GENERAL_COMMAND, b"sn-get", 4.0)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.sd
@pytest.mark.requirement("REC-001")
@pytest.mark.requirement("REC-002")
def test_usb_recording_control_stays_inactive_and_stops_while_idle(
    usb_client, sd_fixture
) -> None:
    """Proves an idle machine accepts M951 without creating an AVI segment."""

    status = usb_client.exchange(SINGLE_COMMAND, b"?", 4.0)
    snapshots = [frame.payload for frame in status if frame.frame_type == 0x81]
    if not snapshots or not snapshots[-1].startswith(b"<Idle|"):
        pytest.skip("recording control requires an idle physical machine")

    videos_before = _directory_snapshot(usb_client, b"/sd/videos")
    try:
        started = usb_client.exchange(GENERAL_COMMAND, b"M951", 4.0)
        assert any(
            frame.frame_type == GENERAL_COMMAND and frame.payload == b"ok\n"
            for frame in started
        )
        # Observe several recording-task intervals while the physical motion
        # board remains idle. The directory must remain byte-for-byte stable.
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            current = usb_client.exchange(SINGLE_COMMAND, b"?", 4.0)
            current_snapshots = [
                frame.payload for frame in current if frame.frame_type == 0x81
            ]
            assert current_snapshots and current_snapshots[-1].startswith(b"<Idle|")
            time.sleep(0.2)
        assert _directory_snapshot(usb_client, b"/sd/videos") == videos_before
    finally:
        stopped = usb_client.exchange(GENERAL_COMMAND, b"M952", 4.0)
        assert any(
            frame.frame_type == GENERAL_COMMAND and frame.payload == b"ok\n"
            for frame in stopped
        )


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.usb
@pytest.mark.wifi
@pytest.mark.requirement("NET-030")
def test_usb_wifi_scan_returns_bounded_response(usb_client, tcp_host: str) -> None:
    def abort_if_stuck(_signum, _frame):
        raise TimeoutError("USB WLAN scan exceeded its 30-second budget")

    previous_handler = signal.signal(signal.SIGALRM, abort_if_stuck)
    signal.setitimer(signal.ITIMER_REAL, 30.0)
    try:
        _test_usb_wifi_scan_returns_bounded_response(usb_client, tcp_host)
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
        signal.signal(signal.SIGALRM, previous_handler)


def _test_usb_wifi_scan_returns_bounded_response(usb_client, tcp_host: str) -> None:
    frames = usb_client.exchange(GENERAL_COMMAND, b"wlan", 15.0)
    assert frames
    assert sum(len(frame.payload) for frame in frames) <= 1024

    # A physical station scan can briefly defer new TCP handshakes even after
    # its USB response has been queued. Prove bounded service recovery here so
    # the immediately following cross-transport case does not inherit scan
    # settling time and report it as an ownership failure.
    deadline = time.monotonic() + 10.0
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((tcp_host, 2222), timeout=1.0):
                return
        except OSError:
            time.sleep(0.25)
    pytest.fail("TCP service did not recover within ten seconds after WLAN scan")


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.usb
@pytest.mark.tcp
@pytest.mark.requirement("OWN-001")
def test_usb_and_tcp_read_requests_can_coexist(usb_client, tcp_host: str) -> None:
    with ThreadPoolExecutor(max_workers=2) as executor:
        usb_result = executor.submit(
            usb_client.exchange, GENERAL_COMMAND, b"ftype /", 4.0
        )
        tcp_result = executor.submit(
            TcpProtocolClient(tcp_host).exchange,
            GENERAL_COMMAND,
            b"ftype /",
            4.0,
        )
        assert usb_result.result()
        assert tcp_result.result()


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.http
@pytest.mark.wifi
@pytest.mark.requirement("NET-DIAG-001")
def test_wifi_diagnostic_counters_are_monotonic(tcp_host: str) -> None:
    def read() -> dict:
        with urllib.request.urlopen(
            f"http://{tcp_host}/api/wifi/diagnostics", timeout=5.0
        ) as response:
            return json.load(response)

    first = read()
    second = read()
    for key in (
        "station_starts", "associations", "disconnections",
        "addresses_acquired", "addresses_lost",
    ):
        assert second[key] >= first[key]


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.wifi
@pytest.mark.requirement("DISC-004")
@pytest.mark.requirement("NET-020")
def test_station_disconnect_event_is_observed_and_automatically_recovers(
    usb_client, tcp_host: str,
) -> None:
    """Uses native USB to survive the intentional station outage."""

    def diagnostics() -> dict:
        with urllib.request.urlopen(
            f"http://{tcp_host}/api/wifi/diagnostics", timeout=5.0
        ) as response:
            return json.load(response)

    before = diagnostics()
    reply = _payloads(
        usb_client.exchange(GENERAL_COMMAND, b"wlan -d", 5.0)
    )
    assert b"WiFi Disconnected!\n" in reply

    deadline = time.monotonic() + 30.0
    while time.monotonic() < deadline:
        try:
            after = diagnostics()
        except OSError:
            time.sleep(0.2)
            continue
        if (
            after["disconnections"] > before["disconnections"]
            and after["addresses_acquired"] > before["addresses_acquired"]
        ):
            return
        time.sleep(0.2)
    pytest.fail("station disconnect/reconnect events were not both observed")
