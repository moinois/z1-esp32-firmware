#!/usr/bin/env python3
"""Validate and resolve the approved LPC release-payload catalog."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import urllib.request
from pathlib import Path

SEMVER = re.compile(r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
OPTION_START = "# BEGIN GENERATED CONTROLLER PAYLOAD OPTIONS"
OPTION_END = "# END GENERATED CONTROLLER PAYLOAD OPTIONS"


def load_catalog(path: Path) -> dict[str, dict[str, object]]:
    document = json.loads(path.read_text(encoding="utf-8"))
    if document.get("schema_version") != 1 or not isinstance(document.get("payloads"), dict):
        raise ValueError("unsupported controller payload catalog")
    payloads = document["payloads"]
    required = {"family", "display_name", "firmware_version", "repository", "tag", "asset", "sha256"}
    for payload_id, payload in payloads.items():
        if not re.fullmatch(r"[a-z0-9][a-z0-9.-]*", payload_id):
            raise ValueError(f"invalid payload id: {payload_id}")
        missing = required - payload.keys()
        if missing:
            raise ValueError(f"{payload_id} missing fields: {', '.join(sorted(missing))}")
        match = SEMVER.fullmatch(str(payload["firmware_version"]))
        if match is None:
            raise ValueError(f"{payload_id} has invalid firmware_version")
        if payload["tag"] != f"v{payload['firmware_version']}":
            raise ValueError(f"{payload_id} tag does not match firmware_version")
        if not SHA256.fullmatch(str(payload["sha256"])):
            raise ValueError(f"{payload_id} has invalid sha256")
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", str(payload["repository"])):
            raise ValueError(f"{payload_id} has invalid repository")
        if not re.fullmatch(r"[A-Za-z0-9_.-]+", str(payload["asset"])):
            raise ValueError(f"{payload_id} has invalid asset")
        if any(int(component) > 255 for component in match.groups()[:3]):
            raise ValueError(f"{payload_id} version components must fit one byte")
    return payloads


def encoded_version(version: str) -> int:
    match = SEMVER.fullmatch(version)
    if match is None:
        raise ValueError("invalid firmware version")
    major, minor, patch = (int(value) for value in match.groups()[:3])
    return (major << 16) | (minor << 8) | patch


def workflow_options(text: str) -> list[str]:
    generated = text.split(OPTION_START, 1)[1].split(OPTION_END, 1)[0]
    return re.findall(r"^\s+- (.+?)\s*$", generated, re.MULTILINE)


def resolve_payload(payloads: dict[str, dict[str, object]], selection: str) -> tuple[str, dict[str, object]]:
    if selection in payloads:
        return selection, payloads[selection]
    matches = [(payload_id, payload) for payload_id, payload in payloads.items()
               if payload["display_name"] == selection]
    if len(matches) != 1:
        raise ValueError(f"unknown or ambiguous controller payload: {selection}")
    return matches[0]


def download_payload(payload: dict[str, object], directory: Path) -> Path:
    directory.mkdir(parents=True, exist_ok=True)
    destination = directory / str(payload["asset"])
    url = (f"https://github.com/{payload['repository']}/releases/download/"
           f"{payload['tag']}/{payload['asset']}")
    with urllib.request.urlopen(url) as response, destination.open("wb") as output:
        output.write(response.read())
    actual = hashlib.sha256(destination.read_bytes()).hexdigest()
    if actual != payload["sha256"]:
        destination.unlink()
        raise ValueError(f"controller payload SHA-256 mismatch: {actual}")
    return destination


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("payload_id")
    parser.add_argument("--catalog", type=Path, default=Path("release/controller-payloads.json"))
    parser.add_argument("--github-output", type=Path)
    parser.add_argument("--download-directory", type=Path)
    args = parser.parse_args()
    payloads = load_catalog(args.catalog)
    try:
        payload_id, payload = resolve_payload(payloads, args.payload_id)
        if args.download_directory:
            download_payload(payload, args.download_directory)
    except ValueError as error:
        raise SystemExit(str(error)) from error
    values = {**payload, "payload_id": payload_id,
              "encoded_version": f"0x{encoded_version(str(payload['firmware_version'])):08X}"}
    output = "\n".join(f"{key}={value}" for key, value in values.items()) + "\n"
    if args.github_output:
        with args.github_output.open("a", encoding="utf-8") as stream:
            stream.write(output)
    else:
        print(output, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
