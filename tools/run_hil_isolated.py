"""Run HIL groups sequentially so reset tests cannot poison later groups."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
GROUPS = (
    (
        "readonly",
        ("tests/hardware", "-m", "not mutating and not destructive and not ble"),
    ),
    ("mutating", ("tests/hardware", "-m", "mutating and not ble")),
    ("destructive", ("tests/hardware", "-m", "destructive and not ble")),
)
# A group may contain deliberate protocol timeouts, but must not hold the
# transport hostage indefinitely. The read-only transport regression currently
# needs about 131 seconds and the mutating endurance group exceeds 307 seconds
# on physical Wi-Fi, so retain a measured whole-group margin while every test
# remains protected by pytest's independent ten-minute ceiling.
GROUP_TIMEOUT_SECONDS = 900


def run_group(name: str, pytest_args: tuple[str, ...]) -> int:
    """Runs one pytest group and waits for its process to terminate."""
    command = [sys.executable, "-m", "pytest", "-q", *pytest_args]
    print(f"\n=== HIL group: {name} ===", flush=True)
    try:
        completed = subprocess.run(
            command,
            cwd=ROOT,
            env=os.environ.copy(),
            timeout=GROUP_TIMEOUT_SECONDS,
            check=False,
        )
    except subprocess.TimeoutExpired:
        print(
            f"HIL group {name} exceeded {GROUP_TIMEOUT_SECONDS}s; "
            "stopping before the next group.",
            file=sys.stderr,
        )
        return 124
    if completed.returncode != 0:
        print(
            f"HIL group {name} exited with status {completed.returncode}; "
            "later groups were not started.",
            file=sys.stderr,
        )
    return completed.returncode


def main() -> int:
    """Runs readonly, mutating, and destructive groups without overlap."""
    for name, pytest_args in GROUPS:
        result = run_group(name, pytest_args)
        if result:
            return result
    # CoreBluetooth on macOS can retain a scan session after disconnect. Run
    # every BLE case in its own process so that backend state cannot leak to
    # the next case. The marker filters still preserve the safety gates.
    for marker in ("readonly", "mutating", "destructive"):
        collect = subprocess.run(
            [sys.executable, "-m", "pytest", "tests/hardware/test_ble_blufi.py",
             "--collect-only", "-q", "-m", marker],
            cwd=ROOT, env=os.environ.copy(), text=True, capture_output=True,
            check=False,
        )
        if collect.returncode:
            return collect.returncode
        nodes = [line.strip() for line in collect.stdout.splitlines()
                 if "::" in line and line.strip().startswith("tests/")]
        for node in nodes:
            result = run_group(f"ble:{marker}:{node.rsplit('::', 1)[-1]}",
                               (node,))
            if result:
                return result
    print("\nAll HIL groups completed.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
