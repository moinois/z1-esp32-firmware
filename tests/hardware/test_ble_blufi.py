"""Physical BLE advertising and standard BLUFI GATT schema validation."""

from __future__ import annotations

import asyncio
import hashlib
import http.client
import json
import os
import time
from typing import Any

import pytest

BLUFI_NAME_PREFIX = "MK_"
BLUFI_SERVICE = "0000ffff-0000-1000-8000-00805f9b34fb"
BLUFI_WRITE = "0000ff01-0000-1000-8000-00805f9b34fb"
BLUFI_NOTIFY = "0000ff02-0000-1000-8000-00805f9b34fb"
BLUFI_SALT = bytes.fromhex(
    "5a315f424c5546495f53414c545f32303235aab1a30688453667908721701182"
)


def _require_ble_fixture() -> None:
    if os.getenv("Z1_HIL_BLE") != "1":
        pytest.skip("declare the host BLE adapter with Z1_HIL_BLE=1")


def _expected_blufi_name() -> str | None:
    machine_name = os.getenv("Z1_HIL_MACHINE_NAME")
    if machine_name is None:
        return None
    return BLUFI_NAME_PREFIX + machine_name.encode()[:23].decode(
        errors="surrogateescape"
    )


def _matches_blufi_name(name: str | None) -> bool:
    if name is None:
        return False
    expected = _expected_blufi_name()
    return name == expected if expected is not None else name.startswith(BLUFI_NAME_PREFIX)


def _wait_for_diagnostic_port(previous: str, timeout: float = 20.0) -> str:
    """Returns the re-enumerated serial port after a target reset.

    USB CDC device names are not stable across an ESP32 reset.  Waiting for
    the old path to disappear and selecting the newly enumerated port avoids
    sending subsequent diagnostics to a stale file descriptor.
    """
    from serial.tools import list_ports

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        candidates = [
            port.device
            for port in list_ports.comports()
            if port.device != previous and port.vid == 0x1A86 and port.pid == 0x55F3
        ]
        if len(candidates) == 1:
            return candidates[0]
        time.sleep(0.25)
    raise AssertionError("firmware diagnostic USB port did not re-enumerate")


async def _find_blufi():
    from bleak import BleakScanner
    from bleak.exc import BleakError

    try:
        devices = await BleakScanner.discover(timeout=10.0, return_adv=True)
    except BleakError as error:
        pytest.skip(
            "host BLE adapter is unavailable to Python; enable Bluetooth "
            f"permission for the terminal/Codex process: {error}"
        )
    for device, advertisement in devices.values():
        if _matches_blufi_name(device.name) or _matches_blufi_name(
            advertisement.local_name
        ):
            return device, advertisement
    return None


async def _require_blufi():
    # CoreBluetooth can occasionally leave a scan future pending after a
    # disconnect.  Bound the complete operation, not just Bleak's requested
    # scan duration, so one fixture cannot stall the HIL process.
    try:
        found = await asyncio.wait_for(_find_blufi(), timeout=15.0)
    except asyncio.TimeoutError as error:
        raise AssertionError("BLUFI scan did not complete within 15 seconds") from error
    expected = _expected_blufi_name() or f"{BLUFI_NAME_PREFIX}<machine-name>"
    assert found is not None, f"{expected} was not advertised within 10 seconds"
    return found


async def _assert_gatt_healthy(client: Any) -> None:
    assert client.is_connected
    assert bytes(await client.read_gatt_char(BLUFI_NOTIFY)) == b"\x00"


async def _request_frames(
    client: Any, request: bytes, timeout: float = 5.0
) -> list[bytes]:
    response: asyncio.Future[list[bytes]] = asyncio.get_running_loop().create_future()
    frames: list[bytes] = []

    def receive(_characteristic: Any, value: bytearray) -> None:
        frame = bytes(value)
        frames.append(frame)
        if len(frame) >= 2 and not frame[1] & 0x10 and not response.done():
            response.set_result(frames.copy())

    await client.start_notify(BLUFI_NOTIFY, receive)
    try:
        await client.write_gatt_char(BLUFI_WRITE, request, response=True)
        return await asyncio.wait_for(response, timeout=timeout)
    finally:
        await client.stop_notify(BLUFI_NOTIFY)


async def _request_notification_count(
    client: Any, request: bytes, count: int, timeout: float = 5.0
) -> list[bytes]:
    response: asyncio.Future[list[bytes]] = asyncio.get_running_loop().create_future()
    frames: list[bytes] = []

    def receive(_characteristic: Any, value: bytearray) -> None:
        frames.append(bytes(value))
        if len(frames) == count and not response.done():
            response.set_result(frames.copy())

    await client.start_notify(BLUFI_NOTIFY, receive)
    try:
        await client.write_gatt_char(BLUFI_WRITE, request, response=True)
        return await asyncio.wait_for(response, timeout=timeout)
    finally:
        await client.stop_notify(BLUFI_NOTIFY)


async def _request_frame(client: Any, request: bytes, timeout: float = 5.0) -> bytes:
    frames = await _request_frames(client, request, timeout)
    assert len(frames) == 1, f"expected one BLUFI response frame, received {len(frames)}"
    return frames[0]


def _assert_plain_frame(frame: bytes, subtype: int, sequence: int = 0) -> bytes:
    assert len(frame) >= 4
    assert frame[0] == ((subtype << 2) | 0x01)
    assert frame[1] == 0x04
    assert frame[2] == sequence
    assert frame[3] == len(frame) - 4
    return frame[4:]


def _reassemble_plain_frames(frames: list[bytes], subtype: int) -> bytes:
    assert frames
    payload = bytearray()
    for index, frame in enumerate(frames):
        assert len(frame) >= 4
        assert frame[0] == ((subtype << 2) | 0x01)
        assert frame[1] & 0x04
        assert frame[1] & ~0x14 == 0
        assert frame[2] == index
        assert frame[3] == len(frame) - 4
        fragment = frame[4:]
        if frame[1] & 0x10:
            assert len(fragment) >= 2
            fragment = fragment[2:]
        elif index != len(frames) - 1:
            pytest.fail("final BLUFI fragment arrived before the last frame")
        payload.extend(fragment)
    return bytes(payload)


def _crc16(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else crc << 1
    return crc ^ 0xFFFF


def _dh_parameter(value: bytes) -> bytes:
    return len(value).to_bytes(2, "big") + value


def _decrypt_protected_frame(
    frame: bytes, subtype: int, key: bytes, sequence: int
) -> bytes:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

    assert len(frame) >= 6
    assert frame[0] == ((subtype << 2) | 0x01)
    assert frame[1] in (0x07, 0x17)
    assert frame[2] == sequence
    data_length = frame[3]
    assert len(frame) == 4 + data_length + 2
    iv = bytes([sequence]) + bytes(15)
    decryptor = Cipher(algorithms.AES(key), modes.CFB(iv)).decryptor()
    plaintext = decryptor.update(frame[4 : 4 + data_length]) + decryptor.finalize()
    received_crc = int.from_bytes(frame[-2:], "little")
    assert received_crc == _crc16(bytes([sequence, data_length]) + plaintext)
    return plaintext


def _protected_input_frame(
    frame_type: int, subtype: int, sequence: int, payload: bytes, key: bytes
) -> bytes:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

    iv = bytes([sequence]) + bytes(15)
    encryptor = Cipher(algorithms.AES(key), modes.CFB(iv)).encryptor()
    encrypted = encryptor.update(payload) + encryptor.finalize()
    checksum = _crc16(bytes([sequence, len(payload)]) + payload).to_bytes(2, "little")
    return bytes(
        [((subtype << 2) | frame_type), 0x03, sequence, len(payload)]
    ) + encrypted + checksum


async def _negotiate_security(client: Any) -> bytes:
    prime = int("fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f", 16)
    prime_bytes = prime.to_bytes(32, "big")
    private = int.from_bytes(os.urandom(32), "big") % (prime - 3) + 2
    public_bytes = pow(2, private, prime).to_bytes(32, "big")
    parameters = (
        _dh_parameter(prime_bytes)
        + _dh_parameter(b"\x02")
        + _dh_parameter(public_bytes)
    )
    assert len(parameters) <= 128

    length_payload = b"\x00" + len(parameters).to_bytes(2, "big")
    await client.write_gatt_char(
        BLUFI_WRITE,
        bytes([0x01, 0x00, 0x00, len(length_payload)]) + length_payload,
        response=True,
    )
    await asyncio.sleep(0.1)
    parameter_payload = b"\x01" + parameters
    negotiation = await _request_frame(
        client,
        bytes([0x01, 0x00, 0x01, len(parameter_payload)]) + parameter_payload,
    )
    server_public_bytes = _assert_plain_frame(negotiation, 0x00)
    assert 0 < len(server_public_bytes) <= 128
    server_public = int.from_bytes(server_public_bytes, "big")
    assert 1 < server_public < prime

    shared_value = pow(server_public, private, prime)
    shared = shared_value.to_bytes(
        max(1, (shared_value.bit_length() + 7) // 8), "big"
    )
    transformed = bytes(
        value ^ BLUFI_SALT[index % len(BLUFI_SALT)]
        for index, value in enumerate(shared)
    )
    key = hashlib.md5(transformed).digest()
    await client.write_gatt_char(
        BLUFI_WRITE, b"\x04\x00\x02\x01\x03", response=True
    )
    return key


def _wifi_diagnostics(host: str) -> dict[str, Any]:
    connection = http.client.HTTPConnection(host, 80, timeout=3.0)
    try:
        connection.request("GET", "/api/wifi/diagnostics")
        response = connection.getresponse()
        body = response.read()
        assert response.status == 200
        return json.loads(body)
    finally:
        connection.close()


def _assert_wifi_records(payload: bytes) -> None:
    offset = 0
    records = 0
    while offset < len(payload):
        record_length = payload[offset]
        assert record_length >= 1
        record_end = offset + 1 + record_length
        assert record_end <= len(payload)
        rssi = int.from_bytes(payload[offset + 1 : offset + 2], signed=True)
        ssid = payload[offset + 2 : record_end]
        assert -127 <= rssi <= 0
        assert len(ssid) <= 32
        records += 1
        offset = record_end
    assert records > 0


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BLE-001")
@pytest.mark.requirement("BLE-002")
@pytest.mark.requirement("BWF-001")
@pytest.mark.requirement("BWF-002")
def test_blufi_advertises_required_identity_and_service() -> None:
    _require_ble_fixture()
    found = asyncio.run(_require_blufi())
    device, advertisement = found
    advertised_name = advertisement.local_name or device.name
    assert advertised_name is not None
    expected_name = _expected_blufi_name()
    if expected_name is not None:
        assert advertised_name == expected_name
    assert advertised_name.startswith(BLUFI_NAME_PREFIX)
    suffix_size = len(advertised_name.encode()) - len(BLUFI_NAME_PREFIX)
    assert 0 <= suffix_size <= 23
    advertised = {value.lower() for value in advertisement.service_uuids}
    if suffix_size <= 16:
        assert BLUFI_SERVICE in advertised
    else:
        assert BLUFI_SERVICE not in advertised


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BWF-003")
def test_blufi_exposes_standard_gatt_schema_and_fixed_read() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        found = await _require_blufi()
        device, _ = found
        async with BleakClient(device, timeout=10.0) as client:
            assert client.is_connected
            service = client.services.get_service(BLUFI_SERVICE)
            assert service is not None
            writable = client.services.get_characteristic(BLUFI_WRITE)
            outgoing = client.services.get_characteristic(BLUFI_NOTIFY)
            assert writable is not None
            assert outgoing is not None
            assert "write" in writable.properties
            assert "notify" in outgoing.properties
            assert client.mtu_size >= 23
            assert writable.max_write_without_response_size >= 20
            assert bytes(await client.read_gatt_char(outgoing)) == b"\x00"

    asyncio.run(asyncio.wait_for(validate(), timeout=45.0))


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BLE-003")
def test_blufi_resumes_advertising_after_disconnect() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        client = BleakClient(device, timeout=10.0)
        await client.connect()
        await _assert_gatt_healthy(client)
        await client.disconnect()
        assert not client.is_connected
        await asyncio.sleep(0.5)
        assert await asyncio.wait_for(_find_blufi(), timeout=15.0) is not None, (
            "the machine-named BLUFI device did not resume advertising after disconnect"
        )

    asyncio.run(asyncio.wait_for(validate(), timeout=45.0))


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BLE-003")
def test_blufi_survives_repeated_connection_cycles() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        for cycle in range(3):
            device, _ = await _require_blufi()
            async with BleakClient(device, timeout=10.0) as client:
                await _assert_gatt_healthy(client)
            assert not client.is_connected, f"BLE cycle {cycle + 1} did not disconnect"
            await asyncio.sleep(0.25)

    asyncio.run(validate())


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BWF-003")
@pytest.mark.requirement("BWF-007")
def test_blufi_notification_subscription_and_invalid_write_keep_gatt_healthy() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        notifications: list[bytes] = []
        async with BleakClient(device, timeout=10.0) as client:
            await client.start_notify(
                BLUFI_NOTIFY,
                lambda _characteristic, value: notifications.append(bytes(value)),
            )
            # BWF-007 requires the ATT write response to remain successful even
            # though this three-byte value is not a complete BLUFI envelope.
            await client.write_gatt_char(BLUFI_WRITE, b"\x01\x02\x03", response=True)
            await asyncio.sleep(0.2)
            await _assert_gatt_healthy(client)
            await client.stop_notify(BLUFI_NOTIFY)
        assert isinstance(notifications, list)

    asyncio.run(validate())


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BWF-040")
def test_blufi_version_request_has_exact_wire_response() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        async with BleakClient(device, timeout=10.0) as client:
            response = await _request_frame(client, b"\x1c\x00\x00\x00")
            assert _assert_plain_frame(response, 0x10) == b"\x01\x03"

    asyncio.run(validate())


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BLE-013")
@pytest.mark.requirement("BWF-043")
def test_blufi_status_request_returns_structured_wifi_report() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        async with BleakClient(device, timeout=10.0) as client:
            response = await _request_frame(client, b"\x14\x00\x00\x00")
            payload = _assert_plain_frame(response, 0x0F)
            assert len(payload) >= 3
            assert payload[0] in (1, 2, 3)
            assert payload[1] in (0, 1, 2)
            assert payload[2] == 0

    asyncio.run(validate())


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.wifi
@pytest.mark.requirement("BLE-015")
@pytest.mark.requirement("BWF-044")
def test_blufi_wifi_list_request_returns_well_formed_records() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        async with BleakClient(device, timeout=10.0) as client:
            frames = await _request_frames(
                client, b"\x24\x00\x00\x00", timeout=15.0
            )
            payload = _reassemble_plain_frames(frames, 0x11)
            _assert_wifi_records(payload)

    asyncio.run(validate())


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BLE-017")
def test_blufi_unknown_control_subtype_has_no_response_and_session_recovers() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        async with BleakClient(device, timeout=10.0) as client:
            with pytest.raises(asyncio.TimeoutError):
                await _request_frame(client, b"\xfc\x00\x00\x00", timeout=1.0)
            response = await _request_frame(client, b"\x1c\x00\x01\x00")
            assert _assert_plain_frame(response, 0x10) == b"\x01\x03"

    asyncio.run(validate())


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BWF-012")
@pytest.mark.requirement("BWF-042")
def test_blufi_sequence_error_does_not_block_correct_retry() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        async with BleakClient(device, timeout=10.0) as client:
            sequence_error = await _request_frame(client, b"\x1c\x00\x01\x00")
            assert _assert_plain_frame(sequence_error, 0x12) == b"\x00"
            version = await _request_frame(client, b"\x1c\x00\x00\x00")
            assert _assert_plain_frame(version, 0x10, sequence=1) == b"\x01\x03"

    asyncio.run(validate())


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BWF-033")
def test_blufi_acknowledges_before_processing_requested_frame() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        async with BleakClient(device, timeout=10.0) as client:
            responses = await _request_notification_count(
                client, b"\x1c\x08\x00\x00", count=2
            )
            assert responses[0] == b"\x00\x04\x00\x01\x00"
            assert _assert_plain_frame(responses[1], 0x10, sequence=1) == b"\x01\x03"

    asyncio.run(validate())


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BWF-020")
@pytest.mark.requirement("BWF-021")
@pytest.mark.requirement("BWF-042")
def test_blufi_rejects_incorrect_checksum_with_exact_error() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        async with BleakClient(device, timeout=10.0) as client:
            checksum_error = await _request_frame(
                client, b"\x1c\x02\x00\x00\x00\x00"
            )
            assert _assert_plain_frame(checksum_error, 0x12) == b"\x01"

    asyncio.run(validate())


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BLESEC-001")
@pytest.mark.requirement("BLESEC-006")
@pytest.mark.requirement("BWF-042")
def test_blufi_invalid_security_negotiation_reports_exact_errors() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        async with BleakClient(device, timeout=10.0) as client:
            too_short = await _request_frame(client, b"\x01\x00\x00\x01\x00")
            assert _assert_plain_frame(too_short, 0x12) == b"\x09"

            zero_length = await _request_frame(
                client, b"\x01\x00\x01\x03\x00\x00\x00"
            )
            assert _assert_plain_frame(zero_length, 0x12, sequence=1) == b"\x05"

    asyncio.run(validate())


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BLESEC-001")
@pytest.mark.requirement("BLESEC-002")
@pytest.mark.requirement("BLESEC-003")
@pytest.mark.requirement("BLESEC-004")
@pytest.mark.requirement("BLESEC-005")
@pytest.mark.requirement("BWF-020")
@pytest.mark.requirement("BWF-021")
@pytest.mark.requirement("BWF-022")
@pytest.mark.requirement("BWF-023")
@pytest.mark.requirement("BWF-030")
@pytest.mark.requirement("BWF-031")
def test_blufi_negotiates_key_and_returns_protected_status() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        async with BleakClient(device, timeout=10.0) as client:
            key = await _negotiate_security(client)
            protected = await _request_frame(client, b"\x14\x00\x03\x00")
            payload = _decrypt_protected_frame(protected, 0x0F, key, sequence=1)
            assert len(payload) >= 3
            assert payload[0] in (1, 2, 3)
            assert payload[1] in (0, 1, 2)
            assert payload[2] == 0

            protected_list = await _request_frames(
                client, b"\x24\x00\x04\x00", timeout=15.0
            )
            list_payload = bytearray()
            for index, frame in enumerate(protected_list):
                fragment = _decrypt_protected_frame(
                    frame, 0x11, key, sequence=2 + index
                )
                if frame[1] & 0x10:
                    assert len(fragment) >= 2
                    fragment = fragment[2:]
                list_payload.extend(fragment)
            _assert_wifi_records(bytes(list_payload))

    asyncio.run(validate())


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.ble
@pytest.mark.wifi
@pytest.mark.requirement("BLE-010")
@pytest.mark.requirement("BLE-011")
def test_blufi_provisions_declared_wifi_credentials() -> None:
    _require_ble_fixture()
    ssid = os.getenv("Z1_HIL_WIFI_SSID")
    password = os.getenv("Z1_HIL_WIFI_PASSWORD")
    host = os.getenv("Z1_HIL_HOST")
    if not ssid or password is None or not host:
        pytest.skip(
            "set Z1_HIL_WIFI_SSID, Z1_HIL_WIFI_PASSWORD, and Z1_HIL_HOST "
            "for mutating BLUFI provisioning"
        )
    ssid_bytes = ssid.encode()
    password_bytes = password.encode()
    assert len(ssid_bytes) <= 31
    assert len(password_bytes) <= 63

    async def provision() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        async with BleakClient(device, timeout=10.0) as client:
            key = await _negotiate_security(client)
            await client.write_gatt_char(
                BLUFI_WRITE,
                _protected_input_frame(1, 0x02, 3, ssid_bytes, key),
                response=True,
            )
            await client.write_gatt_char(
                BLUFI_WRITE,
                _protected_input_frame(1, 0x03, 4, password_bytes, key),
                response=True,
            )
            await client.write_gatt_char(
                BLUFI_WRITE, b"\x0c\x00\x05\x00", response=True
            )

    asyncio.run(provision())
    deadline = time.monotonic() + 30.0
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        try:
            diagnostics = _wifi_diagnostics(host)
            assert diagnostics.get("connected") is True
            return
        except (OSError, AssertionError, json.JSONDecodeError) as error:
            last_error = error
            time.sleep(1.0)
    pytest.fail(f"provisioned station did not become reachable: {last_error}")


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.http
def test_blufi_remains_responsive_during_http_diagnostics() -> None:
    _require_ble_fixture()
    host = os.getenv("Z1_HIL_HOST")
    if not host:
        pytest.skip("set Z1_HIL_HOST for concurrent BLE/HTTP validation")

    async def validate() -> None:
        from bleak import BleakClient

        device, _ = await _require_blufi()
        async with BleakClient(device, timeout=10.0) as client:
            key = await _negotiate_security(client)
            http_reads = asyncio.gather(
                *(asyncio.to_thread(_wifi_diagnostics, host) for _ in range(8))
            )
            protected = await _request_frame(client, b"\x14\x00\x03\x00")
            payload = _decrypt_protected_frame(protected, 0x0F, key, sequence=1)
            diagnostics = await http_reads
            assert len(payload) >= 3
            assert all(isinstance(item, dict) for item in diagnostics)

    asyncio.run(validate())


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.ble
@pytest.mark.diagnostics
@pytest.mark.requirement("BLE-003")
def test_blufi_recovers_advertising_after_target_reset() -> None:
    _require_ble_fixture()
    serial_port = os.getenv("Z1_HIL_SERIAL")
    if not serial_port:
        pytest.skip("set Z1_HIL_SERIAL for BLE reset recovery validation")

    async def validate() -> None:
        import serial
        from bleak import BleakClient

        def pulse_reset() -> None:
            with serial.Serial(
                serial_port, 115200, timeout=0.25, dsrdtr=False, rtscts=False
            ) as device:
                device.dtr = False
                device.rts = True
                time.sleep(0.1)
                device.rts = False

        device, _ = await _require_blufi()
        disconnected = asyncio.Event()
        client = BleakClient(
            device,
            timeout=10.0,
            disconnected_callback=lambda _client: disconnected.set(),
        )
        await client.connect()
        try:
            await _assert_gatt_healthy(client)
            await asyncio.to_thread(pulse_reset)
            await asyncio.to_thread(_wait_for_diagnostic_port, serial_port)
            await asyncio.wait_for(disconnected.wait(), timeout=10.0)
        finally:
            if client.is_connected:
                await client.disconnect()
        await asyncio.sleep(6.0)
        assert await asyncio.wait_for(_find_blufi(), timeout=15.0) is not None, (
            "the machine-named BLUFI device did not advertise after target reset"
        )

    asyncio.run(validate())
