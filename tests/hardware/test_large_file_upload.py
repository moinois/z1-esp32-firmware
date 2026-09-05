"""On-demand physical-SD verification for uploads of at least 100 MiB."""

from __future__ import annotations

import hashlib
import os

import pytest

from tests.hardware.hil_file_transfer import upload_file
from tests.hardware.hil_protocol import GENERAL_COMMAND


MINIMUM_TEST_SIZE = 100 * 1024 * 1024
DEFAULT_TEST_SIZE = MINIMUM_TEST_SIZE
TEST_PATH = "/sd/LARGE100.BIN"


def _fixture_bytes(size: int) -> bytes:
    """Builds deterministic, non-compressible-enough content of an exact size."""

    pattern = bytes((index * 73 + 19) & 0xFF for index in range(4096))
    return (pattern * ((size + len(pattern) - 1) // len(pattern)))[:size]


def _payload(frames) -> bytes:
    """Joins command response payloads for a transport-independent assertion."""

    return b"".join(frame.payload for frame in frames)


@pytest.mark.hardware
@pytest.mark.mutating
@pytest.mark.large_file
@pytest.mark.sd
@pytest.mark.timeout(7200)
def test_upload_of_at_least_one_hundred_mib_reaches_physical_sd(
    sd_client, physical_sd_fixture
) -> None:
    """Uploads at least 100 MiB and verifies the complete target-side digest."""

    assert os.environ["Z1_ALLOW_MUTATION"] == "1"
    requested_size = int(os.getenv("Z1_HIL_LARGE_UPLOAD_BYTES", DEFAULT_TEST_SIZE))
    if requested_size < MINIMUM_TEST_SIZE:
        pytest.fail("Z1_HIL_LARGE_UPLOAD_BYTES must be at least 100 MiB")

    content = _fixture_bytes(requested_size)
    expected_digest = hashlib.md5(content).hexdigest().encode("ascii")
    try:
        upload_file(sd_client, TEST_PATH, content)
        response = sd_client.exchange(
            GENERAL_COMMAND,
            f"md5sum {TEST_PATH}".encode("ascii"),
            timeout_seconds=30.0,
        )
        assert expected_digest in _payload(response).lower()
    finally:
        try:
            sd_client.exchange(
                GENERAL_COMMAND,
                f"rm {TEST_PATH}".encode("ascii"),
                timeout_seconds=10.0,
            )
        except Exception:
            pass
