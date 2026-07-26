"""Read-only physical checks for the framed TCP control service."""

from __future__ import annotations

import socket

import pytest

from tests.hardware.hil_protocol import GENERAL_COMMAND


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.tcp
@pytest.mark.requirement("TCP-001")
@pytest.mark.requirement("TCP-002")
def test_tcp_service_accepts_connection(tcp_host: str) -> None:
    try:
        with socket.create_connection((tcp_host, 2222), timeout=2.0):
            pass
    except OSError as error:
        pytest.skip(f"Makera Z1 TCP service not detected at {tcp_host}:2222: {error}")


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.tcp
@pytest.mark.requirement("TCP-003")
@pytest.mark.requirement("TCP-004")
@pytest.mark.requirement("FILE-026")
def test_tcp_read_only_round_trip(tcp_client) -> None:
    try:
        frames = tcp_client.exchange(
            GENERAL_COMMAND, b"ftype /sd/config.txt", timeout_seconds=4.0
        )
    except OSError as error:
        pytest.skip(f"Makera Z1 TCP service unavailable: {error}")
    assert frames, "TCP service returned no valid frame for a read-only request"
    assert any(frame.payload for frame in frames), "all TCP responses were empty"
