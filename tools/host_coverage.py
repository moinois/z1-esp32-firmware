#!/usr/bin/env python3
"""Build host tests and generate a local LLVM code-coverage report."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path
from typing import Sequence


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build" / "host-coverage"
REPORT_DIR = BUILD_DIR / "coverage"
PROFILE_DATA = BUILD_DIR / "coverage.profdata"
TEST_BINARY = BUILD_DIR / "tests" / "core_tests"


def run(command: Sequence[str], *, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, env=env, check=True)


def find_llvm_tool(name: str) -> str:
    """Find an LLVM tool through xcrun or PATH."""

    xcrun = shutil.which("xcrun")
    if xcrun:
        result = subprocess.run(
            [xcrun, "--find", name],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        if result.returncode == 0:
            path = result.stdout.strip()
            if path:
                return path

    path = shutil.which(name)
    if path:
        return path

    raise RuntimeError(
        f"{name} was not found; install or select the Xcode Command Line Tools"
    )


def main() -> int:
    try:
        if shutil.which("cmake") is None:
            raise RuntimeError(
                "cmake is unavailable.\n\n"
                "Before running this command, activate the ESP-IDF environment:\n\n"
                "    source ~/esp/esp-idf/export.sh"
            )
        if shutil.which("ctest") is None:
            raise RuntimeError(
                "cmake is unavailable.\n\n"
                "Before running this command, activate the ESP-IDF environment:\n\n"
                "    source ~/esp/esp-idf/export.sh"
            )

        llvm_profdata = find_llvm_tool("llvm-profdata")
        llvm_cov = find_llvm_tool("llvm-cov")

        # Remove stale profile data so it cannot contaminate the new report.
        for profile in BUILD_DIR.glob("coverage-*.profraw"):
            profile.unlink()

        if PROFILE_DATA.exists():
            PROFILE_DATA.unlink()

        if REPORT_DIR.exists():
            shutil.rmtree(REPORT_DIR)

        run(["cmake", "--preset", "host-coverage"])
        run(["cmake", "--build", "--preset", "host-coverage"])
        run(["ctest", "--preset", "host-coverage"])

        raw_profiles = sorted(BUILD_DIR.glob("coverage-*.profraw"))
        if not raw_profiles:
            raise RuntimeError("the tests produced no LLVM profile data")

        run(
            [
                llvm_profdata,
                "merge",
                "-sparse",
                *[str(path) for path in raw_profiles],
                "-o",
                str(PROFILE_DATA),
            ]
        )

        REPORT_DIR.mkdir(parents=True, exist_ok=True)

        run(
            [
                llvm_cov,
                "show",
                str(TEST_BINARY),
                f"-instr-profile={PROFILE_DATA}",
                "-format=html",
                f"-output-dir={REPORT_DIR}",
                "-show-line-counts-or-regions",
                "-show-branches=count",
                "-show-expansions",
                "-ignore-filename-regex=(/tests/|/usr/|/Library/)",
            ]
        )

        print()
        run(
            [
                llvm_cov,
                "report",
                str(TEST_BINARY),
                f"-instr-profile={PROFILE_DATA}",
                "-ignore-filename-regex=(/tests/|/usr/|/Library/)",
            ]
        )

        report = REPORT_DIR / "index.html"

        print()
        print("Coverage completed successfully.")
        print()
        print("Coverage report:")
        print(f"  {report.relative_to(ROOT)}")
        print()
        print("Open with:")
        print(f"  open {report.relative_to(ROOT)}")

        return 0

    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())