"""Discovery, safety gates, and requirement reporting for HIL tests."""

from __future__ import annotations

import json
import http.client
import os
from pathlib import Path
from typing import Any, Dict, Iterator, List

import pytest

from tests.hardware.hil_protocol import (
    TcpProtocolClient,
    UsbProtocolClient,
    find_native_usb_device,
)


def pytest_addoption(parser: pytest.Parser) -> None:
    """Registers the optional requirement-level JSON evidence destination."""

    group = parser.getgroup("Makera Z1 hardware")
    group.addoption(
        "--hil-report",
        metavar="PATH",
        help="write requirement-level HIL PASS/FAIL/SKIP results as JSON",
    )


def pytest_configure(config: pytest.Config) -> None:
    """Initializes per-session storage for hardware requirement outcomes."""

    config._z1_hil_results = []  # type: ignore[attr-defined]


def pytest_collection_modifyitems(items: List[pytest.Item]) -> None:
    """Applies explicit environment gates to mutating and destructive cases."""

    allow_mutation = os.getenv("Z1_ALLOW_MUTATION") == "1"
    allow_destructive = os.getenv("Z1_ALLOW_DESTRUCTIVE") == "1"
    for item in items:
        if item.get_closest_marker("destructive") and not allow_destructive:
            item.add_marker(pytest.mark.skip(
                reason="destructive HIL test requires Z1_ALLOW_DESTRUCTIVE=1"
            ))
        elif item.get_closest_marker("mutating") and not allow_mutation:
            item.add_marker(pytest.mark.skip(
                reason="mutating HIL test requires Z1_ALLOW_MUTATION=1"
            ))


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item: pytest.Item, call: pytest.CallInfo[Any]) -> Iterator[None]:
    """Collects terminal hardware outcomes together with requirement markers."""

    outcome = yield
    report = outcome.get_result()
    if not item.get_closest_marker("hardware"):
        return
    if report.when == "setup" and report.passed:
        return
    if report.when not in ("setup", "call"):
        return
    requirements = [
        str(value)
        for marker in item.iter_markers("requirement")
        for value in marker.args
    ]
    status = "PASS" if report.passed else "SKIP" if report.skipped else "FAIL"
    item.config._z1_hil_results.append({  # type: ignore[attr-defined]
        "test": item.nodeid,
        "requirements": requirements,
        "status": status,
        "detail": str(report.longrepr) if not report.passed else "",
    })


def pytest_sessionfinish(session: pytest.Session) -> None:
    """Writes the optional deterministic JSON evidence report after all tests."""

    destination = session.config.getoption("--hil-report")
    if not destination:
        return
    path = Path(destination)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(session.config._z1_hil_results, indent=2, sort_keys=True) + "\n",  # type: ignore[attr-defined]
        encoding="utf-8",
    )


@pytest.fixture
def usb_device() -> Iterator[Any]:
    """Provides a fresh handle because USB-reset tests invalidate old handles."""

    device, reason = find_native_usb_device()
    if device is None:
        pytest.skip(reason or "native USB device unavailable")
    try:
        yield device
    finally:
        try:
            usb_util = __import__("usb.util", fromlist=["dispose_resources"])
            usb_util.dispose_resources(device)
        except Exception:
            pass


@pytest.fixture
def usb_client(usb_device: Any) -> UsbProtocolClient:
    """Wraps the current native USB handle or skips when it cannot be claimed."""

    try:
        return UsbProtocolClient(usb_device)
    except Exception as error:
        pytest.skip(f"native USB interface could not be opened: {error}")


@pytest.fixture(scope="session")
def sd_fixture() -> None:
    """Requires an explicitly declared physical or mock SD capability."""

    if os.getenv("Z1_HIL_SD") != "1" and os.getenv("Z1_HIL_MOCK_SD") != "1":
        pytest.skip(
            "SD storage not declared with Z1_HIL_SD=1 or Z1_HIL_MOCK_SD=1"
        )


@pytest.fixture
def sd_client(request: pytest.FixtureRequest, tcp_host: str) -> Any:
    """Uses native USB when present, otherwise an explicitly reachable TCP target."""
    device, _ = find_native_usb_device()
    if device is not None:
        # Reuse this test's PyUSB interface. Opening a second handle to the same
        # interface fails with "Access denied" on macOS.
        return request.getfixturevalue("usb_client")
    if os.getenv("Z1_HIL_HOST"):
        return TcpProtocolClient(tcp_host)
    pytest.skip("SD HIL requires native USB or an explicit Z1_HIL_HOST")


@pytest.fixture(scope="session")
def tcp_host() -> str:
    """Returns the explicitly configured target or its provisioning-AP address."""

    return os.getenv("Z1_HIL_HOST", "192.168.4.1")


@pytest.fixture(scope="session")
def tcp_client(tcp_host: str) -> TcpProtocolClient:
    """Creates a fresh framed TCP client for one test function."""

    return TcpProtocolClient(tcp_host)


@pytest.fixture(scope="session")
def network_mock_fixture() -> None:
    """Requires firmware built with the deterministic network fault adapter."""

    if os.getenv("Z1_HIL_MOCK_NETWORK") != "1":
        pytest.skip("network fault HIL requires Z1_HIL_MOCK_NETWORK=1")


@pytest.fixture(scope="session")
def camera_fixture(tcp_host: str) -> None:
    """Detects an initialized camera through its public runtime API."""
    connection = http.client.HTTPConnection(tcp_host, 80, timeout=5.0)
    try:
        connection.request(
            "POST",
            "/api/camera/resolution",
            body=b'{"resolution":10}',
            headers={"Content-Type": "application/json"},
        )
        response = connection.getresponse()
        body = response.read()
    except OSError as error:
        pytest.skip(f"camera capability endpoint unavailable: {error}")
    finally:
        connection.close()
    if response.status == 200:
        return
    if response.status == 500 and body == b"Failed to set framesize":
        pytest.skip("camera module not detected by the firmware")
    pytest.fail(
        f"unexpected camera capability response: HTTP {response.status} {body!r}"
    )
