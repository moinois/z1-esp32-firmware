"""Physical BLE advertising and standard BLUFI GATT schema validation."""

from __future__ import annotations

import asyncio
import os

import pytest

BLUFI_NAME = "BLUFI_DEVICE"
BLUFI_SERVICE = "0000ffff-0000-1000-8000-00805f9b34fb"
BLUFI_WRITE = "0000ff01-0000-1000-8000-00805f9b34fb"
BLUFI_NOTIFY = "0000ff02-0000-1000-8000-00805f9b34fb"


def _require_ble_fixture() -> None:
    if os.getenv("Z1_HIL_BLE") != "1":
        pytest.skip("declare the host BLE adapter with Z1_HIL_BLE=1")


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
        if device.name == BLUFI_NAME or advertisement.local_name == BLUFI_NAME:
            return device, advertisement
    return None


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BLE-001")
@pytest.mark.requirement("BLE-002")
@pytest.mark.requirement("BWF-001")
@pytest.mark.requirement("BWF-002")
def test_blufi_advertises_required_identity_and_service() -> None:
    _require_ble_fixture()
    found = asyncio.run(_find_blufi())
    assert found is not None, f"{BLUFI_NAME} was not advertised within 10 seconds"
    _, advertisement = found
    advertised = {value.lower() for value in advertisement.service_uuids}
    assert BLUFI_SERVICE in advertised


@pytest.mark.hardware
@pytest.mark.readonly
@pytest.mark.ble
@pytest.mark.requirement("BWF-003")
def test_blufi_exposes_standard_gatt_schema_and_fixed_read() -> None:
    _require_ble_fixture()

    async def validate() -> None:
        from bleak import BleakClient

        found = await _find_blufi()
        assert found is not None, f"{BLUFI_NAME} was not advertised within 10 seconds"
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
            assert bytes(await client.read_gatt_char(outgoing)) == b"\x00"

    asyncio.run(validate())
