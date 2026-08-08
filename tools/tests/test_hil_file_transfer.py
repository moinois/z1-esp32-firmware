"""Tests the target HIL file-transfer client without requiring hardware."""

from __future__ import annotations

import hashlib
import unittest

from tests.hardware.hil_file_transfer import (
    FILE_COMMAND,
    FILE_COMPLETE,
    FILE_DATA,
    FILE_GEOMETRY,
    FILE_MD5,
    FILE_RETRY,
    download_file,
    upload_file,
)
from tests.hardware.hil_protocol import ReceivedFrame


class ScriptedClient:
    """Returns scripted frames while recording every host request."""

    def __init__(self, responses: list[list[ReceivedFrame]]) -> None:
        """Copies the ordered responses consumed by subsequent exchanges."""

        self.responses = list(responses)
        self.requests: list[tuple[int, bytes]] = []

    def exchange(
        self, frame_type: int, payload: bytes, timeout_seconds: float = 3.0
    ) -> list[ReceivedFrame]:
        """Records the request and returns the next scripted response batch."""

        self.requests.append((frame_type, payload))
        return self.responses.pop(0)


class HilFileTransferTests(unittest.TestCase):
    """Verifies transport-independent upload/download sequencing and retries."""

    def test_upload_sends_md5_geometry_and_requested_blocks(self) -> None:
        data = b"abcdef"
        client = ScriptedClient(
            [
                [ReceivedFrame(FILE_MD5, b"")],
                [ReceivedFrame(FILE_GEOMETRY, b"")],
                [ReceivedFrame(FILE_DATA, (1).to_bytes(4, "big"))],
                [ReceivedFrame(FILE_DATA, (2).to_bytes(4, "big"))],
                [ReceivedFrame(FILE_COMPLETE, b"ok\r\n")],
            ]
        )

        upload_file(client, "/round.bin", data, block_size=3)

        self.assertEqual(client.requests[0], (FILE_COMMAND, b"upload /round.bin"))
        self.assertEqual(
            client.requests[1],
            (FILE_MD5, hashlib.md5(data).hexdigest().encode("ascii")),
        )
        self.assertEqual(client.requests[2], (FILE_GEOMETRY, (2).to_bytes(4, "big")))
        self.assertEqual(client.requests[3], (FILE_DATA, b"\0\0\0\1abc"))
        self.assertEqual(client.requests[4], (FILE_DATA, b"\0\0\0\2def"))

    def test_download_uses_announced_geometry_and_verifies_md5(self) -> None:
        data = b"abcdef"
        digest = hashlib.md5(data).hexdigest().encode("ascii")
        client = ScriptedClient(
            [
                [ReceivedFrame(FILE_MD5, digest)],
                [ReceivedFrame(FILE_GEOMETRY, b"\0\0\0\2\0\3")],
                [ReceivedFrame(FILE_DATA, b"\0\0\0\1abc")],
                [ReceivedFrame(FILE_DATA, b"\0\0\0\2def")],
                [ReceivedFrame(FILE_COMPLETE, b"ok\r\n")],
            ]
        )

        self.assertEqual(download_file(client, "/round.bin"), data)
        self.assertEqual(client.requests[0], (FILE_COMMAND, b"download /round.bin"))
        self.assertEqual(client.requests[1], (FILE_GEOMETRY, b""))
        self.assertEqual(client.requests[-1], (FILE_COMPLETE, b""))

    def test_upload_continues_from_timed_retry_at_known_phase(self) -> None:
        data = b"abc"
        retry = [ReceivedFrame(FILE_RETRY, b"Info: need retry!")]
        client = ScriptedClient(
            [
                retry,
                retry,
                retry,
                retry,
                [ReceivedFrame(FILE_COMPLETE, b"ok\r\n")],
            ]
        )

        upload_file(client, "/retry.bin", data)

        self.assertEqual(
            client.requests.count((FILE_DATA, b"\0\0\0\1abc")), 2
        )

    def test_download_can_skip_md5_for_an_internally_appended_debug_log(self) -> None:
        client = ScriptedClient(
            [
                [ReceivedFrame(FILE_MD5, b"0" * 32)],
                [ReceivedFrame(FILE_GEOMETRY, b"\0\0\0\1\0\3")],
                [ReceivedFrame(FILE_DATA, b"\0\0\0\1log")],
                [ReceivedFrame(FILE_COMPLETE, b"ok\r\n")],
            ]
        )

        self.assertEqual(
            download_file(client, "/serial.log", verify_md5=False), b"log"
        )


if __name__ == "__main__":
    unittest.main()
