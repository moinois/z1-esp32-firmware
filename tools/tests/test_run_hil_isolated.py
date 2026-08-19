"""Tests that the isolated HIL runner never broadens a selected test node."""

from __future__ import annotations

import subprocess
import sys
from unittest.mock import patch

from tools import run_hil_isolated


def test_run_group_uses_the_exact_pytest_selection() -> None:
    """Keeps a BLE node isolated instead of also collecting tests/hardware."""

    node = "tests/hardware/test_ble_blufi.py::test_one_case"
    completed = subprocess.CompletedProcess([], 0)
    with patch.object(subprocess, "run", return_value=completed) as run:
        assert run_hil_isolated.run_group("ble:readonly:test_one_case", (node,)) == 0

    command = run.call_args.args[0]
    assert command == [sys.executable, "-m", "pytest", "-q", node]


def test_standard_groups_explicitly_select_the_hardware_directory() -> None:
    """Documents that only aggregate groups receive the broad directory path."""

    assert all(arguments[0] == "tests/hardware" for _, arguments in run_hil_isolated.GROUPS)
