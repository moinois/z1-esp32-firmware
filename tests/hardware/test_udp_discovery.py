"""Physical UDP discovery checks over the station network."""

from __future__ import annotations

import socket
import time

import pytest

from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    encode_frame,
    receive_tcp_frames,
)


DISCOVERY_PORT = 3333
TCP_CONTROL_PORT = 2222
CAPACITY_TRANSITION_TIMEOUT_SECONDS = 8.0


def _discovery_for_target(target: str, tcp_full: str, timeout: float = 4.0) -> list[str]:
    """Returns one validated discovery record emitted by the selected target."""

    listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, "SO_REUSEPORT"):
        # Permit the conformance listener to coexist with MakeraStudio's
        # production discovery listener on hosts such as macOS.
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    listener.bind(("", DISCOVERY_PORT))
    listener.settimeout(0.25)
    deadline = time.monotonic() + timeout
    try:
        while time.monotonic() < deadline:
            try:
                payload, _ = listener.recvfrom(128)
            except socket.timeout:
                continue
            text = payload.decode("utf-8")
            fields = text.split(",")
            if len(fields) != 5 or fields[1] != target:
                continue
            assert not text.endswith(("\r", "\n"))
            assert fields[0]
            assert fields[2] == str(TCP_CONTROL_PORT)
            assert fields[4]
            if fields[3] != tcp_full:
                # A datagram emitted immediately before the TCP transition can
                # already be queued in the host network stack. Wait for the
                # next 500 ms cycle to advertise the requested state.
                continue
            return fields
    finally:
        listener.close()
    pytest.fail(
        f"no discovery from {target} with tcp-full={tcp_full} within {timeout}s"
    )


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.udp
@pytest.mark.requirement("DISC-001")
@pytest.mark.requirement("DISC-002")
@pytest.mark.requirement("DISC-004")
def test_periodic_station_discovery_payload(tcp_host: str) -> None:
    """Receives the periodic station broadcast and validates its wire fields."""

    _discovery_for_target(tcp_host, "0")


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.udp
@pytest.mark.tcp
@pytest.mark.requirement("DISC-001")
def test_discovery_reports_exact_tcp_capacity(tcp_host: str) -> None:
    """Advertises tcp-full only while all four TCP slots are occupied."""

    # A preceding case can have closed its host sockets while the target is
    # still inside the normative receive/keepalive window. Start only after
    # those client tasks have had time to release their logical slots.
    time.sleep(10.5)
    clients: list[socket.socket] = []
    try:
        for _ in range(4):
            client = socket.create_connection(
                (tcp_host, TCP_CONTROL_PORT), timeout=5.0
            )
            client.sendall(encode_frame(GENERAL_COMMAND, b"ftype /"))
            frames = receive_tcp_frames(client, 3.0)
            assert frames
            assert all(frame.frame_type != 0x91 for frame in frames)
            clients.append(client)
        # UDP delivery is intentionally best-effort. Observe several normative
        # 500 ms discovery cycles so a busy shared host port cannot turn one
        # dropped broadcast into a false TCP-capacity failure.
        _discovery_for_target(
            tcp_host, "1", timeout=CAPACITY_TRANSITION_TIMEOUT_SECONDS
        )
    finally:
        for client in clients:
            client.close()

    # The target closes each client task asynchronously. Allow one periodic
    # cycle before requiring the next advertisement to show free capacity.
    time.sleep(0.5)
    _discovery_for_target(
        tcp_host, "0", timeout=CAPACITY_TRANSITION_TIMEOUT_SECONDS
    )
