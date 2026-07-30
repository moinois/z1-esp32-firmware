"""Physical UDP discovery checks over the station network."""

from __future__ import annotations

import socket
import time

import pytest


DISCOVERY_PORT = 3333
TCP_CONTROL_PORT = 2222


def _discovery_for_target(target: str, tcp_full: str, timeout: float = 4.0) -> list[str]:
    """Returns one validated discovery record emitted by the selected target."""

    listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
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

    clients: list[socket.socket] = []
    try:
        for _ in range(4):
            clients.append(
                socket.create_connection((tcp_host, TCP_CONTROL_PORT), timeout=5.0)
            )
        _discovery_for_target(tcp_host, "1")
    finally:
        for client in clients:
            client.close()

    # The target closes each client task asynchronously. Allow one periodic
    # cycle before requiring the next advertisement to show free capacity.
    time.sleep(0.5)
    _discovery_for_target(tcp_host, "0")
