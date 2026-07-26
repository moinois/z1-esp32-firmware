"""Tests for the aggregate firmware packaging tool."""

from __future__ import annotations

import binascii
import struct
import tempfile
import unittest
from pathlib import Path

from tools.package_firmware import (
    build_firmware_package,
    build_mainboard_package,
    main,
)


class PackageFirmwareTests(unittest.TestCase):
    def test_mainboard_only_header_and_payload_are_exact(self) -> None:
        image = b"\xE9\x02\x03\x04"

        package = build_mainboard_package(image, mainboard_version=0x12345678)

        self.assertEqual(len(package), 32 + len(image))
        self.assertEqual(struct.unpack_from("<I", package, 0x00)[0], 0x4D5173EE)
        self.assertEqual(package[0x04:0x08], bytes((1, 32, 1, 0)))
        self.assertEqual(struct.unpack_from("<I", package, 0x08)[0], len(image))
        self.assertEqual(struct.unpack_from("<I", package, 0x0C)[0], 0)
        self.assertEqual(struct.unpack_from("<I", package, 0x10)[0], 0x12345678)
        self.assertEqual(struct.unpack_from("<I", package, 0x14)[0], 0)
        self.assertEqual(package[32:], image)

    def test_crc_fields_match_independent_golden_calculation(self) -> None:
        image = b"\xE9mainboard-image"
        package = build_mainboard_package(image)

        header_crc = binascii.crc32(package[:0x18]) & 0xFFFFFFFF
        file_crc = binascii.crc32(package[:0x1C] + package[0x20:]) & 0xFFFFFFFF

        self.assertEqual(struct.unpack_from("<I", package, 0x18)[0], header_crc)
        self.assertEqual(struct.unpack_from("<I", package, 0x1C)[0], file_crc)

    def test_combined_package_sets_controller_fields_and_payload_order(self) -> None:
        mainboard = b"\xE9mainboard"
        controller = b"controller-firmware"

        package = build_firmware_package(
            mainboard,
            mainboard_version=7,
            controller_image=controller,
            controller_version=11,
        )

        self.assertEqual(package[0x06], 0x03)
        self.assertEqual(struct.unpack_from("<I", package, 0x08)[0], len(mainboard))
        self.assertEqual(struct.unpack_from("<I", package, 0x0C)[0], len(controller))
        self.assertEqual(struct.unpack_from("<I", package, 0x10)[0], 7)
        self.assertEqual(struct.unpack_from("<I", package, 0x14)[0], 11)
        self.assertEqual(package[32:], mainboard + controller)
        self.assertEqual(
            struct.unpack_from("<I", package, 0x1C)[0],
            binascii.crc32(package[:0x1C] + package[0x20:]) & 0xFFFFFFFF,
        )

    def test_controller_only_package_has_only_controller_flag_and_payload(self) -> None:
        controller = b"controller-only"

        package = build_firmware_package(
            b"", controller_image=controller, controller_version=19
        )

        self.assertEqual(package[0x06], 0x02)
        self.assertEqual(struct.unpack_from("<I", package, 0x08)[0], 0)
        self.assertEqual(struct.unpack_from("<I", package, 0x0C)[0], len(controller))
        self.assertEqual(struct.unpack_from("<I", package, 0x10)[0], 0)
        self.assertEqual(struct.unpack_from("<I", package, 0x14)[0], 19)
        self.assertEqual(package[32:], controller)

    def test_rejects_empty_non_esp_and_out_of_range_version(self) -> None:
        with self.assertRaisesRegex(ValueError, "empty"):
            build_mainboard_package(b"")
        with self.assertRaisesRegex(ValueError, "0xE9"):
            build_mainboard_package(b"not-an-esp-image")
        with self.assertRaisesRegex(ValueError, "unsigned 32-bit"):
            build_mainboard_package(b"\xE9", mainboard_version=0x1_0000_0000)
        with self.assertRaisesRegex(ValueError, "requires a controller image"):
            build_firmware_package(b"\xE9", controller_version=1)
        with self.assertRaisesRegex(ValueError, "must contain"):
            build_firmware_package(b"")
        with self.assertRaisesRegex(ValueError, "requires a mainboard image"):
            build_firmware_package(
                b"", mainboard_version=1, controller_image=b"controller"
            )

    def test_cli_defaults_are_overridable_and_write_atomically(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "mainboard.bin"
            output = root / "nested" / "firmware.bin"
            source.write_bytes(b"\xE9payload")

            result = main(
                [
                    "--mainboard",
                    str(source),
                    "--output",
                    str(output),
                    "--mainboard-version",
                    "0x2A",
                ]
            )

            self.assertEqual(result, 0)
            package = output.read_bytes()
            self.assertEqual(package[32:], b"\xE9payload")
            self.assertEqual(struct.unpack_from("<I", package, 0x10)[0], 42)
            self.assertFalse(output.with_name(output.name + ".tmp").exists())

    def test_cli_adds_controller_image_and_version(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            mainboard = root / "mainboard.bin"
            controller = root / "controller_firmware.bin"
            output = root / "firmware.bin"
            mainboard.write_bytes(b"\xE9main")
            controller.write_bytes(b"controller")

            result = main(
                [
                    "--mainboard",
                    str(mainboard),
                    "--controller",
                    str(controller),
                    "--controller-version",
                    "0x2B",
                    "--output",
                    str(output),
                ]
            )

            self.assertEqual(result, 0)
            package = output.read_bytes()
            self.assertEqual(package[0x06], 0x03)
            self.assertEqual(struct.unpack_from("<I", package, 0x0C)[0], 10)
            self.assertEqual(struct.unpack_from("<I", package, 0x14)[0], 43)
            self.assertEqual(package[32:], b"\xE9maincontroller")

    def test_cli_rejects_controller_version_without_image(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "mainboard.bin"
            source.write_bytes(b"\xE9main")

            self.assertEqual(
                main(
                    [
                        "--mainboard",
                        str(source),
                        "--controller-version",
                        "1",
                    ]
                ),
                1,
            )

    def test_cli_builds_controller_only_package(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            controller = root / "controller_firmware.bin"
            output = root / "firmware.bin"
            controller.write_bytes(b"controller-only")

            result = main(
                [
                    "--controller",
                    str(controller),
                    "--controller-version",
                    "23",
                    "--output",
                    str(output),
                ]
            )

            self.assertEqual(result, 0)
            package = output.read_bytes()
            self.assertEqual(package[0x06], 0x02)
            self.assertEqual(struct.unpack_from("<I", package, 0x08)[0], 0)
            self.assertEqual(struct.unpack_from("<I", package, 0x0C)[0], 15)
            self.assertEqual(struct.unpack_from("<I", package, 0x14)[0], 23)
            self.assertEqual(package[32:], b"controller-only")

    def test_cli_requires_at_least_one_image(self) -> None:
        self.assertEqual(main([]), 1)

    def test_cli_rejects_mainboard_version_without_image(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            controller = Path(directory) / "controller.bin"
            controller.write_bytes(b"controller")
            self.assertEqual(
                main(
                    [
                        "--controller",
                        str(controller),
                        "--mainboard-version",
                        "1",
                    ]
                ),
                1,
            )


if __name__ == "__main__":
    unittest.main()
