"""Concurrent target validation of TCP-local command routing."""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import os
import time

import pytest

from tests.hardware.hil_protocol import (
    GENERAL_COMMAND,
    SINGLE_COMMAND,
    ReceivedFrame,
    TcpProtocolClient,
)


def _exchange(
    host: str, frame_type: int, command: bytes, timeout: float = 5.0
) -> list[ReceivedFrame]:
    """Runs one command on an independent TCP connection."""

    return TcpProtocolClient(host).exchange(frame_type, command, timeout)


def _matching_payload(frames: list[ReceivedFrame], frame_type: int) -> bytes:
    """Returns the response payload of the requested type."""

    matches = [frame.payload for frame in frames if frame.frame_type == frame_type]
    assert len(matches) == 1, frames
    return matches[0]


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.tcp
@pytest.mark.controller
@pytest.mark.requirement("CMD-004")
@pytest.mark.requirement("ROUTE-001")
@pytest.mark.requirement("ROUTE-018")
@pytest.mark.requirement("REC-001")
@pytest.mark.requirement("RUN-010")
@pytest.mark.requirement("RUN-040")
def test_four_tcp_slots_route_local_families_to_their_origin(tcp_host: str) -> None:
    """Runs four local families concurrently and checks origin-bound replies."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    if os.getenv("Z1_HIL_MOCK_CONTROLLER") != "1":
        pytest.skip("deterministic status requires the controller mock")

    requests = (
        (GENERAL_COMMAND, b"M951"),
        (GENERAL_COMMAND, b"sn-get"),
        (GENERAL_COMMAND, b"sys-time"),
        (SINGLE_COMMAND, b"?"),
    )
    with ThreadPoolExecutor(max_workers=4) as executor:
        futures = [
            executor.submit(_exchange, tcp_host, frame_type, command)
            for frame_type, command in requests
        ]
        recording, serial, runtime, status = [future.result() for future in futures]

    assert _matching_payload(recording, GENERAL_COMMAND) == b"ok\n"
    assert _matching_payload(serial, 0x83).startswith(b"sn = ")
    assert _matching_payload(runtime, 0x83).startswith(b"sys-time-data = ")
    assert _matching_payload(status, 0x81).startswith(b"<Idle|")

    # Stop the recording request as part of the test instead of leaking the
    # mutating M951 state into subsequent camera or storage cases.
    stopped = _exchange(tcp_host, GENERAL_COMMAND, b"M952")
    assert _matching_payload(stopped, GENERAL_COMMAND) == b"ok\n"


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.tcp
@pytest.mark.controller
@pytest.mark.requirement("ROUTE-001")
@pytest.mark.requirement("ROUTE-018")
@pytest.mark.requirement("UART-003")
@pytest.mark.requirement("RUN-010")
def test_tcp_controller_bridge_returns_controller_originated_reply(
    tcp_host: str,
) -> None:
    """Traverses TCP, controller TX/RX, local serial service, and UART reply."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    if os.getenv("Z1_HIL_MOCK_CONTROLLER") != "1":
        pytest.skip("controller bridge validation requires the controller mock")

    # This command is forwarded to the selected controller and deliberately
    # has no immediate host response. The mock injects an sn-get back through
    # the production UART decoder, then records the mainboard's framed reply.
    assert _exchange(tcp_host, GENERAL_COMMAND, b"mock-command sn-get", 1.0) == []

    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        diagnostic = _matching_payload(
            _exchange(tcp_host, GENERAL_COMMAND, b"diagnose"), 0x82
        )
        if b"CMD:sn-get:OK" in diagnostic:
            assert b"UART:FRAGMENTED" in diagnostic
            return
        time.sleep(0.1)
    pytest.fail("TCP/controller bridge did not return the serial-number reply")
