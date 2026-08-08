#!/usr/bin/env python3
"""Build host tests and generate a local LLVM code-coverage report."""

from __future__ import annotations

import json
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
SUMMARY_PATH = REPORT_DIR / "coverage-summary.json"
BADGE_PATH = REPORT_DIR / "badge.svg"


def line_coverage(exported: dict[str, object]) -> float:
    """Extracts total line coverage from one llvm-cov summary export."""

    data = exported.get("data")
    if not isinstance(data, list) or len(data) != 1:
        raise ValueError("llvm-cov export must contain exactly one data record")
    try:
        percent = data[0]["totals"]["lines"]["percent"]
    except (KeyError, TypeError) as error:
        raise ValueError("llvm-cov export has no total line percentage") from error
    if not isinstance(percent, (int, float)) or not 0.0 <= percent <= 100.0:
        raise ValueError("llvm-cov line percentage is outside 0 through 100")
    return float(percent)


def coverage_color(percent: float) -> str:
    """Maps line coverage to the conventional badge quality palette."""

    for threshold, color in (
        (90.0, "#4c1"),
        (80.0, "#97ca00"),
        (70.0, "#a4a61d"),
        (60.0, "#dfb317"),
        (50.0, "#fe7d37"),
    ):
        if percent >= threshold:
            return color
    return "#e05d44"


def coverage_badge_svg(percent: float) -> str:
    """Builds a dependency-free static SVG badge for the latest release."""

    value = f"{percent:.1f}%"
    color = coverage_color(percent)
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="124" height="20" role="img" aria-label="coverage: {value}">
  <title>coverage: {value}</title>
  <linearGradient id="s" x2="0" y2="100%"><stop offset="0" stop-color="#bbb" stop-opacity=".1"/><stop offset="1" stop-opacity=".1"/></linearGradient>
  <clipPath id="r"><rect width="124" height="20" rx="3" fill="#fff"/></clipPath>
  <g clip-path="url(#r)"><rect width="75" height="20" fill="#555"/><rect x="75" width="49" height="20" fill="{color}"/><rect width="124" height="20" fill="url(#s)"/></g>
  <g fill="#fff" text-anchor="middle" font-family="Verdana,Geneva,DejaVu Sans,sans-serif" font-size="11"><text x="37.5" y="15" fill="#010101" fill-opacity=".3">coverage</text><text x="37.5" y="14">coverage</text><text x="99.5" y="15" fill="#010101" fill-opacity=".3">{value}</text><text x="99.5" y="14">{value}</text></g>
</svg>
"""


def run(command: Sequence[str], *, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, env=env, check=True)


def _compiler_has_standard_library(compiler: str) -> bool:
    """Checks that a candidate compiler can include the C++ standard library."""

    result = subprocess.run(
        [compiler, "-fsyntax-only", "-x", "c++", "-"],
        input="#include <cstddef>\nint main() { return 0; }\n",
        text=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return result.returncode == 0


def _homebrew_llvm_root() -> Path | None:
    """Returns the optional keg-only Homebrew LLVM root."""

    brew = shutil.which("brew")
    if brew is None:
        return None
    result = subprocess.run(
        [brew, "--prefix", "llvm"],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    if result.returncode != 0:
        return None
    return Path(result.stdout.strip())


def find_llvm_toolchain() -> tuple[str, str, str]:
    """Finds one coherent Clang, llvm-profdata, and llvm-cov installation."""

    roots = [root for root in [_homebrew_llvm_root()] if root is not None]
    path_clang = shutil.which("clang++")
    if path_clang:
        roots.append(Path(path_clang).resolve().parent.parent)
    roots.extend(
        path.parent.parent
        for path in Path("/usr/lib").glob("llvm-*/bin/clang++")
    )

    xcrun = shutil.which("xcrun")
    if xcrun:
        result = subprocess.run(
            [xcrun, "--find", "clang++"],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        if result.returncode == 0:
            roots.append(Path(result.stdout.strip()).resolve().parent.parent)

    for root in roots:
        compiler = root / "bin" / "clang++"
        llvm_profdata = root / "bin" / "llvm-profdata"
        llvm_cov = root / "bin" / "llvm-cov"
        if (
            compiler.is_file()
            and llvm_profdata.is_file()
            and llvm_cov.is_file()
            and _compiler_has_standard_library(str(compiler))
        ):
            return str(compiler), str(llvm_profdata), str(llvm_cov)

    raise RuntimeError(
        "no coherent LLVM toolchain with C++ standard-library headers was found; "
        "repair Xcode Command Line Tools or install Homebrew llvm"
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
                "ctest is unavailable.\n\n"
                "Before running this command, activate the ESP-IDF environment:\n\n"
                "    source ~/esp/esp-idf/export.sh"
            )

        compiler, llvm_profdata, llvm_cov = find_llvm_toolchain()

        # Coverage is entirely generated output. Starting clean prevents a
        # previously selected compiler from contaminating the profile format.
        if BUILD_DIR.exists():
            shutil.rmtree(BUILD_DIR)

        run(
            [
                "cmake",
                "--preset",
                "host-coverage",
                f"-DCMAKE_CXX_COMPILER={compiler}",
            ]
        )
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

        exported = subprocess.run(
            [
                llvm_cov,
                "export",
                str(TEST_BINARY),
                f"-instr-profile={PROFILE_DATA}",
                "-summary-only",
                "-ignore-filename-regex=(/tests/|/usr/|/Library/)",
            ],
            cwd=ROOT,
            check=True,
            text=True,
            stdout=subprocess.PIPE,
        ).stdout
        summary = json.loads(exported)
        SUMMARY_PATH.write_text(
            json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        BADGE_PATH.write_text(
            coverage_badge_svg(line_coverage(summary)), encoding="utf-8"
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
        print(f"  {SUMMARY_PATH.relative_to(ROOT)}")
        print(f"  {BADGE_PATH.relative_to(ROOT)}")
        print()
        print("Open with:")
        print(f"  open {report.relative_to(ROOT)}")

        return 0

    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
