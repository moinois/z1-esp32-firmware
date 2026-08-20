"""USB HIL for saved SoftAP commands and live Wi-Fi parameter queries."""

from __future__ import annotations

import os
from pathlib import Path
import re
import time

import pytest

from tests.hardware.hil_protocol import UsbProtocolClient
from tests.hardware.hil_ota import multipart_upload, wait_for_usb_service_restart

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

    frames = client.exchange(GENERAL_COMMAND, b"ap get", 4.0)
    responses = [
        frame.payload.decode("utf-8", errors="strict")
        for frame in frames
        if frame.frame_type == CONSOLE_MESSAGE
    ]
    matches = [
        re.fullmatch(
            r"AP enable=([01]) ssid=(.*?) password=(.*?) channel=(\d+)\r\n",
            response,
        )
        for response in responses
    ]
    match = next((candidate for candidate in reversed(matches) if candidate), None)
    assert match is not None, responses
    return match[1] == "1", match[2], match[3], int(match[4])


def _wait_for_settings(
    client: UsbProtocolClient,
    expected: tuple[bool, str, str, int],
    timeout_seconds: float = 10.0,
) -> None:
    """Ignores reboot broadcasts until the AP command service is responsive."""

    deadline = time.monotonic() + timeout_seconds
    last: object = None
    while time.monotonic() < deadline:
        try:
            last = _settings(client)
            if last == expected:
                return
        except (AssertionError, OSError) as error:
            last = error
        time.sleep(0.2)
    pytest.fail(f"saved SoftAP settings did not settle to {expected!r}: {last!r}")


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


@pytest.mark.hardware
@pytest.mark.destructive
@pytest.mark.requirement("APCFG-001")
@pytest.mark.requirement("APCFG-002")
@pytest.mark.requirement("APCFG-003")
@pytest.mark.requirement("APCFG-004")
def test_saved_softap_settings_are_loaded_across_reboot_and_restored(
    usb_client: UsbProtocolClient, tcp_host: str
) -> None:
    """Reboots through OTA twice and restores every persistent AP setting."""

    image_name = os.getenv("Z1_HIL_OTA_IMAGE")
    if not image_name:
        pytest.skip("set Z1_HIL_OTA_IMAGE to the current application image")
    image = Path(image_name)
    if not image.is_file():
        pytest.skip(f"OTA image fixture does not exist: {image}")

    original = _settings(usb_client)
    temporary = (
        not original[0],
        original[1],
        "HilBoot88" if original[2] != "HilBoot88" else "HilBoot99",
        1 if original[3] != 1 else 2,
    )

    current = usb_client
    try:
        assert _console(current, f"ap channel {temporary[3]}".encode()).startswith(
            "AP channel saved"
        )
        assert _console(
            current, b"ap password " + temporary[2].encode()
        ).startswith("AP password saved")
        _console(current, b"ap enable" if temporary[0] else b"ap disable")

        status, _ = multipart_upload(
            tcp_host, "/update", image.read_bytes(), image.name,
            maximum_attempts=1,
        )
        assert status == 200
        current = wait_for_usb_service_restart(current, timeout_seconds=60)
        _wait_for_settings(current, temporary)
    finally:
        _console(current, f"ap channel {original[3]}".encode())
        if original[2] == "null":
            _console(current, b"ap password clear")
        else:
            _console(current, b"ap password " + original[2].encode())
        _console(current, b"ap enable" if original[0] else b"ap disable")

        status, _ = multipart_upload(
            tcp_host, "/update", image.read_bytes(), image.name,
            maximum_attempts=1,
        )
        assert status == 200
        current = wait_for_usb_service_restart(current, timeout_seconds=60)
        _wait_for_settings(current, original)
        current.close()
