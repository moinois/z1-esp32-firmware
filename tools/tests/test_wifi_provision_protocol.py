"""Tests for the host-side Makera Z1 USB provisioning protocol."""

import unittest

from tools.wifi_provision_protocol import (
    GENERAL_COMMAND,
    OPERATION_FAILURE,
    OPERATION_SUCCESS,
    TEXT_RESPONSE,
    build_wifi_command,
    decode_frames,
    encode_frame,
)


class WifiProvisionProtocolTests(unittest.TestCase):
    """Protects the wire contract used to provision station credentials."""

    def test_build_wifi_command_uses_general_command_frame(self) -> None:
        """Builds the exact command consumed by the firmware WLAN parser."""
        frame = build_wifi_command("Away", "SailWithMe")

        self.assertEqual(frame[0], 0x86)
        self.assertEqual(frame[1], 0x68)
        self.assertEqual(frame[4], GENERAL_COMMAND)
        self.assertEqual(frame[5:-4], b"wlan Away SailWithMe")

    def test_command_escapes_whitespace_and_reserved_bytes(self) -> None:
        """Encodes credentials so firmware tokenization preserves their value."""
        frame = build_wifi_command("Home Network", "p? ss\\word")

        self.assertEqual(frame[5:-4], b"wlan Home\x01Network p\x02\x01ss\\word")

    def test_encode_frame_rejects_oversized_payload(self) -> None:
        """Refuses payloads that cannot fit the firmware length field."""
        with self.assertRaises(ValueError):
            encode_frame(GENERAL_COMMAND, b"x" * 65533)

    def test_decode_frames_returns_complete_frames_and_preserves_partial_data(self) -> None:
        """Decodes concatenated replies while retaining an incomplete tail."""
        success = encode_frame(OPERATION_SUCCESS, b"ok\n")
        response = encode_frame(TEXT_RESPONSE, b"connecting\n")

        frames, remainder = decode_frames(success + response[:4])

        self.assertEqual(frames, [(OPERATION_SUCCESS, b"ok\n")])
        self.assertEqual(remainder, response[:4])

    def test_decode_frames_reports_operation_failure(self) -> None:
        """Keeps failure payloads available to the CLI for a useful error."""
        failure = encode_frame(OPERATION_FAILURE, b"connect_failed\n")

        frames, remainder = decode_frames(failure)

        self.assertEqual(frames, [(OPERATION_FAILURE, b"connect_failed\n")])
        self.assertEqual(remainder, b"")


if __name__ == "__main__":
    unittest.main()
