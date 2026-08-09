"""Run HIL groups sequentially so reset tests cannot poison later groups."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
GROUPS = (
    ("readonly", ("-m", "not mutating and not destructive")),
    ("mutating", ("-m", "mutating")),
    ("destructive", ("-m", "destructive")),
)
# A group may contain deliberate protocol timeouts, but must not hold the
# transport hostage for the ten-minute per-test pytest limit.  The runner's
# shorter bound makes a stuck reset/disconnect group fail closed.
GROUP_TIMEOUT_SECONDS = 120


def run_group(name: str, marker_args: tuple[str, ...]) -> int:
    """Runs one pytest group and waits for its process to terminate."""
    command = [sys.executable, "-m", "pytest", "tests/hardware", "-q", *marker_args]
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
    for name, marker_args in GROUPS:
        result = run_group(name, marker_args)
        if result:
            return result
    print("\nAll HIL groups completed.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
