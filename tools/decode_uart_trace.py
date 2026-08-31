#!/usr/bin/env python3
"""Decode the opt-in /sd/uart-trace.bin controller-link capture."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

HEADER = struct.Struct("<4sBBHQI")
MAGIC = b"Z1UT"


def decode(path: Path) -> None:
    with path.open("rb") as source:
        record = 0
        while header := source.read(HEADER.size):
            record += 1
            if len(header) != HEADER.size:
                raise SystemExit(f"record {record}: truncated header")
            magic, version, direction, size, timestamp_us, sequence = HEADER.unpack(header)
            if magic != MAGIC or version != 1:
                raise SystemExit(f"record {record}: invalid header")
            payload = source.read(size)
            if len(payload) != size:
                raise SystemExit(f"record {record}: truncated payload")
            label = {0: "RX", 1: "TX"}.get(direction, f"?{direction}")
            print(
                f"{timestamp_us:016d} #{sequence:010d} "
                f"{label} {size:4d} {payload.hex(' ')}"
            )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=Path)
    decode(parser.parse_args().trace)


if __name__ == "__main__":
    main()
