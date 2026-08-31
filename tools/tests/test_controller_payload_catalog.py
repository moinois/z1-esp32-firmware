"""Tests for the approved LPC payload catalog."""

from pathlib import Path

from tools.controller_payload_catalog import encoded_version, load_catalog, workflow_options


def test_catalog_and_workflow_options_are_synchronized() -> None:
    catalog = load_catalog(Path("release/controller-payloads.json"))
    workflow = Path(".github/workflows/release-firmware.yml").read_text(encoding="utf-8")
    assert workflow_options(workflow) == ["none", *catalog]


def test_official_payload_identity_and_encoded_version() -> None:
    payload = load_catalog(Path("release/controller-payloads.json"))["official-z1-v1.1.2"]
    assert payload["sha256"] == "50169068a4021795f3424e0423606f323f84026c2705c0a18f010990fca6b18e"
    assert encoded_version(str(payload["firmware_version"])) == 0x00010102
