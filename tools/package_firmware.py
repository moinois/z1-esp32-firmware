#!/usr/bin/env python3
"""Builds specification-compliant aggregate firmware update packages."""

from __future__ import annotations

import argparse
import binascii
import struct
import sys
from pathlib import Path
from typing import Sequence

PACKAGE_MAGIC = 0x4D5173EE
PACKAGE_VERSION = 1
HEADER_SIZE = 32
MAINBOARD_PRESENT = 0x01
CONTROLLER_PRESENT = 0x02
ESP_IMAGE_MAGIC = 0xE9
MAX_U32 = 0xFFFFFFFF
MAGIC_OFFSET = 0x00
FORMAT_VERSION_OFFSET = 0x04
HEADER_LENGTH_OFFSET = 0x05
FLAGS_OFFSET = 0x06
MAINBOARD_SIZE_OFFSET = 0x08
CONTROLLER_SIZE_OFFSET = 0x0C
MAINBOARD_VERSION_OFFSET = 0x10
CONTROLLER_VERSION_OFFSET = 0x14
HEADER_CRC_OFFSET = 0x18
FILE_CRC_OFFSET = 0x1C


def _crc32(data: bytes) -> int:
    """Returns reflected CRC-32/ISO-HDLC as an unsigned 32-bit value."""

    return binascii.crc32(data) & MAX_U32


def build_firmware_package(
    mainboard_image: bytes,
    *,
    mainboard_version: int = 0,
    controller_image: bytes = b"",
    controller_version: int = 0,
) -> bytes:
    """Wraps mainboard and optional controller images in aggregate format."""

    if not mainboard_image and not controller_image:
        raise ValueError("package must contain a mainboard or controller image")
    if mainboard_image and mainboard_image[0] != ESP_IMAGE_MAGIC:
        raise ValueError("mainboard image does not start with ESP image magic 0xE9")
    if len(mainboard_image) > MAX_U32:
        raise ValueError("mainboard image is too large for the package header")
    if not 0 <= mainboard_version <= MAX_U32:
        raise ValueError("mainboard version must fit an unsigned 32-bit field")
    if not mainboard_image and mainboard_version != 0:
        raise ValueError("mainboard version requires a mainboard image")
    if len(controller_image) > MAX_U32:
        raise ValueError("controller image is too large for the package header")
    if not 0 <= controller_version <= MAX_U32:
        raise ValueError("controller version must fit an unsigned 32-bit field")
    if not controller_image and controller_version != 0:
        raise ValueError("controller version requires a controller image")

    header = bytearray(HEADER_SIZE)
    struct.pack_into("<I", header, MAGIC_OFFSET, PACKAGE_MAGIC)
    header[FORMAT_VERSION_OFFSET] = PACKAGE_VERSION
    header[HEADER_LENGTH_OFFSET] = HEADER_SIZE
    header[FLAGS_OFFSET] = (MAINBOARD_PRESENT if mainboard_image else 0) | (
        CONTROLLER_PRESENT if controller_image else 0
    )
    struct.pack_into("<I", header, MAINBOARD_SIZE_OFFSET, len(mainboard_image))
    struct.pack_into("<I", header, CONTROLLER_SIZE_OFFSET, len(controller_image))
    struct.pack_into("<I", header, MAINBOARD_VERSION_OFFSET, mainboard_version)
    struct.pack_into("<I", header, CONTROLLER_VERSION_OFFSET, controller_version)
    struct.pack_into(
        "<I", header, HEADER_CRC_OFFSET, _crc32(bytes(header[:HEADER_CRC_OFFSET]))
    )

    # UPD-012 excludes the stored file-CRC field without restarting the CRC.
    payload = mainboard_image + controller_image
    file_crc_input = bytes(header[:FILE_CRC_OFFSET]) + payload
    struct.pack_into("<I", header, FILE_CRC_OFFSET, _crc32(file_crc_input))
    return bytes(header) + payload


def build_mainboard_package(
    mainboard_image: bytes, *, mainboard_version: int = 0
) -> bytes:
    """Backward-compatible mainboard-only packaging entry point."""

    if not mainboard_image:
        raise ValueError("mainboard image is empty")
    return build_firmware_package(
        mainboard_image, mainboard_version=mainboard_version
    )


def _unsigned_u32(value: str) -> int:
    """Parses decimal or prefixed integer text constrained to a uint32 field."""

    parsed = int(value, 0)
    if not 0 <= parsed <= MAX_U32:
        raise argparse.ArgumentTypeError("value must fit an unsigned 32-bit field")
    return parsed


def _parser() -> argparse.ArgumentParser:
    """Creates the command-line contract without consuming process arguments."""

    parser = argparse.ArgumentParser(
        description="Package mainboard and/or controller firmware for /sd/firmware.bin"
    )
    parser.add_argument(
        "--mainboard",
        type=Path,
        help="optional ESP32-S3 application image",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("build/firmware.bin"),
        help="aggregate output path (default: build/firmware.bin)",
    )
    parser.add_argument(
        "--mainboard-version",
        type=_unsigned_u32,
        default=0,
        help="opaque uint32 metadata, decimal or 0x-prefixed (default: 0)",
    )
    parser.add_argument(
        "--controller",
        type=Path,
        help="optional controller firmware appended after the mainboard image",
    )
    parser.add_argument(
        "--controller-version",
        type=_unsigned_u32,
        default=None,
        help="opaque controller uint32 metadata; requires --controller",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    """Validates image arguments and atomically writes one aggregate package."""

    args = _parser().parse_args(argv)
    try:
        if args.mainboard is None and args.controller is None:
            raise ValueError("at least one of --mainboard or --controller is required")
        if args.mainboard is None and args.mainboard_version != 0:
            raise ValueError("--mainboard-version requires --mainboard")
        mainboard_image = (
            args.mainboard.read_bytes() if args.mainboard is not None else b""
        )
        if args.mainboard is not None and not mainboard_image:
            raise ValueError("mainboard image is empty")
        if args.controller is None and args.controller_version is not None:
            raise ValueError("--controller-version requires --controller")
        controller_image = (
            args.controller.read_bytes() if args.controller is not None else b""
        )
        if args.controller is not None and not controller_image:
            raise ValueError("controller image is empty")
        controller_version = args.controller_version or 0
        package = build_firmware_package(
            mainboard_image,
            mainboard_version=args.mainboard_version,
            controller_image=controller_image,
            controller_version=controller_version,
        )
        args.output.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.output.with_name(args.output.name + ".tmp")
        temporary.write_bytes(package)
        temporary.replace(args.output)
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(
        f"wrote {args.output}: {len(package)} bytes "
        f"(mainboard={len(mainboard_image)}, controller={len(controller_image)}, "
        f"mainboard_version={args.mainboard_version}, "
        f"controller_version={controller_version})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
