"""USB HIL for saved SoftAP commands and live Wi-Fi parameter queries."""

from __future__ import annotations

import re

import pytest

from tests.hardware.hil_protocol import UsbProtocolClient

GENERAL_COMMAND = 0xA2
CONSOLE_MESSAGE = 0x90


def _console(client: UsbProtocolClient, command: bytes) -> str:
    """Returns the single exact console payload for a local command."""

    frames = client.exchange(GENERAL_COMMAND, command, 4.0)
    matches = [frame.payload for frame in frames if frame.frame_type == CONSOLE_MESSAGE]
    assert matches, f"no console response for {command!r}: {frames!r}"
    return matches[-1].decode("utf-8", errors="strict")


def _settings(client: UsbProtocolClient) -> tuple[bool, str, str, int]:
    """Parses the normative `ap get` response without hiding delimiters."""

    response = _console(client, b"ap get")
    match = re.fullmatch(
        r"AP enable=([01]) ssid=(.*?) password=(.*?) channel=(\d+)\r\n",
        response,
    )
    assert match is not None, response
    return match[1] == "1", match[2], match[3], int(match[4])


@pytest.mark.hardware
@pytest.mark.requirement("APCMD-002")
def test_softap_get_has_the_exact_normative_shape(usb_client: UsbProtocolClient) -> None:
    """Checks retained AP state over the native USB command path."""

    enabled, name, password, channel = _settings(usb_client)
    assert isinstance(enabled, bool)
    assert 1 <= len(name.encode("utf-8")) <= 31
    assert password == "null" or 8 <= len(password.encode("utf-8")) <= 63
    assert 1 <= channel <= 11


@pytest.mark.hardware
@pytest.mark.requirement("APQ-001")
@pytest.mark.requirement("APQ-002")
@pytest.mark.requirement("APQ-003")
def test_all_station_and_softap_query_parameters_reply_over_usb(
    usb_client: UsbProtocolClient,
) -> None:
    """Exercises every live M482/M483 value through production adapters."""

    for prefix in ("M482", "M483"):
        for parameter in range(8):
            response = _console(
                usb_client, f"{prefix}.{parameter}".encode("ascii")
            )
            assert response.startswith(f"{prefix} param[{parameter}]:")
            assert response.endswith("\n")


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.requirement("APCFG-005")
@pytest.mark.requirement("APCMD-004")
@pytest.mark.requirement("APCMD-005")
@pytest.mark.requirement("APCMD-006")
def test_reversible_softap_mutations_update_get_and_restore(
    usb_client: UsbProtocolClient,
) -> None:
    """Mutates only settings whose exact prior value can be restored."""

    enabled, name, password, _ = _settings(usb_client)
    temporary_name = "Z1_HIL_AP"
    temporary_password = "HilPass88"
    try:
        assert _console(usb_client, b"ap ssid " + temporary_name.encode()) == (
            "AP ssid saved, apply on reboot\r\n"
        )
        assert _settings(usb_client)[1] == temporary_name

        assert _console(
            usb_client, b"ap password " + temporary_password.encode()
        ) == "AP password saved, apply on reboot\r\n"
        assert _settings(usb_client)[2] == temporary_password

        toggle = b"ap disable" if enabled else b"ap enable"
        expected = "AP disabled\r\n" if enabled else "AP enabled\r\n"
        assert _console(usb_client, toggle) == expected
        assert _settings(usb_client)[0] is not enabled
    finally:
        _console(usb_client, b"ap ssid " + name.encode())
        if password == "null":
            _console(usb_client, b"ap password clear")
        else:
            _console(usb_client, b"ap password " + password.encode())
        _console(usb_client, b"ap enable" if enabled else b"ap disable")
