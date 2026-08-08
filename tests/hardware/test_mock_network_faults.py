"""Target checks for deterministic TCP and discovery socket failures."""

from __future__ import annotations

import socket
import time

import pytest

from tests.hardware.hil_protocol import GENERAL_COMMAND, TcpProtocolClient
from tests.hardware.test_udp_discovery import _discovery_for_target


# General-command text replies use the protocol's 0x83 response packet, not
# the 0xA3 request-family value.
TEXT_RESPONSE = 0x83


def _usb_command(usb_client, command: str) -> bytes:
    """Executes one mock control command and returns its text response."""

    frames = usb_client.exchange(GENERAL_COMMAND, command.encode(), 3.0)
    responses = [frame.payload for frame in frames if frame.frame_type == TEXT_RESPONSE]
    assert responses, f"no text response for {command!r}: {frames!r}"
    return b"".join(responses)


def _wait_until_consumed(usb_client, timeout: float = 4.0) -> None:
    """Waits until the asynchronous discovery task consumes its armed fault."""

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if b"pending: none" in _usb_command(usb_client, "mock-net status"):
            return
        time.sleep(0.1)
    pytest.fail("network fault was not consumed before the deadline")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.tcp
@pytest.mark.requirement("TCP-003")
def test_temporary_tcp_send_failure_retries_complete_frame(
    network_mock_fixture, usb_client, tcp_host: str
) -> None:
    """A temporary send failure is retried without truncating the response."""

    assert b"tcp-temporary-send" in _usb_command(
        usb_client, "mock-net fail-tcp-temporary"
    )
    frames = TcpProtocolClient(tcp_host).exchange(
        GENERAL_COMMAND, b"version", 5.0
    )
    assert frames
    assert b"pending: none" in _usb_command(usb_client, "mock-net status")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.tcp
@pytest.mark.requirement("TCP-003")
def test_permanent_tcp_send_failure_closes_only_affected_session(
    network_mock_fixture, usb_client, tcp_host: str
) -> None:
    """A permanent send failure closes that session and a successor still works."""

    assert b"tcp-permanent-send" in _usb_command(
        usb_client, "mock-net fail-tcp-permanent"
    )
    failed = TcpProtocolClient(tcp_host).exchange(
        GENERAL_COMMAND, b"version", 3.0
    )
    assert failed == []
    time.sleep(0.3)
    assert TcpProtocolClient(tcp_host).exchange(
        GENERAL_COMMAND, b"version", 5.0
    )


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.usb
@pytest.mark.udp
@pytest.mark.requirement("DISC-001")
@pytest.mark.requirement("DISC-008")
@pytest.mark.parametrize(
    "action", ["fail-discovery-open", "fail-discovery-send"]
)
def test_discovery_recovers_after_injected_socket_failure(
    network_mock_fixture, usb_client, tcp_host: str, action: str
) -> None:
    """A failed open or send is consumed and later periodic discovery resumes."""

    assert action.removeprefix("fail-").encode() in _usb_command(
        usb_client, f"mock-net {action}"
    )
    _wait_until_consumed(usb_client)
    _discovery_for_target(tcp_host, "0", timeout=5.0)


@pytest.fixture(autouse=True)
def _clear_fault_after_test(network_mock_fixture, usb_client):
    """Prevents a failed assertion from leaking an armed fault to another test."""

    yield
    try:
        _usb_command(usb_client, "mock-net clear")
    except (AssertionError, OSError, socket.error):
        pass
