"""Runs complete host file transfers through a target's public protocol."""

from __future__ import annotations

import hashlib
import time
from typing import List, Protocol

from tests.hardware.hil_protocol import ReceivedFrame

FILE_COMMAND = 0xB0
FILE_MD5 = 0xB1
FILE_GEOMETRY = 0xB2
FILE_DATA = 0xB3
FILE_COMPLETE = 0xB4
FILE_CANCEL = 0xB5
FILE_RETRY = 0xB6


class FileTransferClient(Protocol):
    """Describes the transport operation required by the transfer driver."""

    def exchange(
        self, frame_type: int, payload: bytes, timeout_seconds: float = 3.0
    ) -> List[ReceivedFrame]:
        """Sends one framed phase packet and returns decoded target responses."""

        ...


class FileTransferError(RuntimeError):
    """Reports an invalid or rejected target transfer response."""


def _exchange_until(
    client: FileTransferClient,
    frame_type: int,
    payload: bytes,
    expected_types: frozenset[int],
    timeout_seconds: float,
) -> List[ReceivedFrame]:
    """Uses terminal-aware transports while retaining simple test doubles."""

    exchange_until = getattr(client, "exchange_until", None)
    if callable(exchange_until):
        return exchange_until(
            frame_type,
            payload,
            expected_types | frozenset({FILE_CANCEL, FILE_RETRY}),
            timeout_seconds,
        )
    return client.exchange(frame_type, payload, timeout_seconds)


def _frame(frames: List[ReceivedFrame], expected_type: int) -> ReceivedFrame:
    """Returns the expected response or raises with the target's best error."""

    for response in frames:
        if response.frame_type == FILE_CANCEL:
            raise FileTransferError(response.payload.decode("utf-8", errors="replace"))
        if response.frame_type == expected_type:
            return response
    observed = ", ".join(f"0x{item.frame_type:02x}" for item in frames) or "none"
    raise FileTransferError(
        f"expected response 0x{expected_type:02x}, received {observed}"
    )


def _prompt(frames: List[ReceivedFrame], expected_type: int) -> ReceivedFrame | None:
    """Accepts either an explicit prompt or a timed retry for the known phase."""

    for response in frames:
        if response.frame_type == FILE_CANCEL:
            raise FileTransferError(response.payload.decode("utf-8", errors="replace"))
        if response.frame_type == expected_type:
            return response
    if any(response.frame_type == FILE_RETRY for response in frames):
        return None
    _frame(frames, expected_type)
    return None


def _send_data_with_timed_retry(
    client: FileTransferClient,
    payload: bytes,
    expected_type: int,
    timeout_seconds: float,
    *,
    maximum_attempts: int = 4,
) -> ReceivedFrame:
    """Resends one data packet when HFT-022 reports a timed retry.

    The command, MD5, and geometry phases deliberately advance after a retry:
    the retry identifies the packet the target is currently waiting for. During
    data transfer that packet remains the current block until the target returns
    either the next sequence request or completion. Duplicate delivery is safe
    because the production receiver recognizes the already-written sequence.
    """

    for _ in range(maximum_attempts):
        responses = _exchange_until(
            client,
            FILE_DATA,
            payload,
            frozenset({expected_type}),
            timeout_seconds,
        )
        prompt = _prompt(responses, expected_type)
        if prompt is not None:
            return prompt
    raise FileTransferError(
        f"target repeated timed retry for response 0x{expected_type:02x}"
    )


def upload_file(
    client: FileTransferClient,
    path: str,
    data: bytes,
    *,
    block_size: int = 1024,
    # Keep a generous host window so a delayed USB response does not leave the
    # target owner active while the test retries.
    timeout_seconds: float = 6.0,
) -> None:
    """Uploads bytes while following target-requested one-based sequencing."""

    if block_size <= 0:
        raise ValueError("block_size must be positive")
    blocks = [data[index : index + block_size] for index in range(0, len(data), block_size)]
    if not blocks:
        blocks = [b""]

    command = f"upload {path}".encode("utf-8")
    responses = _exchange_until(
        client, FILE_COMMAND, command, frozenset({FILE_MD5}), timeout_seconds
    )
    for _ in range(4):
        if any(response.frame_type == FILE_MD5 for response in responses):
            break
        cancellation = next(
            (response for response in responses if response.frame_type == FILE_CANCEL),
            None,
        )
        if cancellation is None:
            break
        # A previous interrupted owner may flush its terminal cancellation on
        # the next exchange, and completion ownership is released
        # asynchronously. Reissue the command for a bounded settling window.
        time.sleep(0.5)
        responses = _exchange_until(
            client, FILE_COMMAND, command, frozenset({FILE_MD5}), timeout_seconds
        )
    _prompt(responses, FILE_MD5)
    responses = _exchange_until(
        client,
        FILE_MD5,
        hashlib.md5(data).hexdigest().encode("ascii"),
        frozenset({FILE_GEOMETRY}),
        timeout_seconds,
    )
    _prompt(responses, FILE_GEOMETRY)
    responses = _exchange_until(
        client,
        FILE_GEOMETRY,
        len(blocks).to_bytes(4, "big"),
        frozenset({FILE_DATA}),
        timeout_seconds,
    )

    for expected_sequence, block in enumerate(blocks, start=1):
        request = _prompt(responses, FILE_DATA)
        if request is not None:
            if len(request.payload) != 4:
                raise FileTransferError("target returned an invalid upload sequence")
            requested_sequence = int.from_bytes(request.payload, "big")
            if requested_sequence != expected_sequence:
                raise FileTransferError(
                    f"target requested block {requested_sequence}, expected {expected_sequence}"
                )
        expected_response = (
            FILE_COMPLETE if expected_sequence == len(blocks) else FILE_DATA
        )
        responses = [
            _send_data_with_timed_retry(
                client,
                expected_sequence.to_bytes(4, "big") + block,
                expected_response,
                timeout_seconds,
            )
        ]


def download_file(
    client: FileTransferClient,
    path: str,
    *,
    timeout_seconds: float = 5.0,
    verify_md5: bool = True,
) -> bytes:
    """Downloads every announced block and verifies its advertised MD5."""

    responses = _exchange_until(
        client,
        FILE_COMMAND,
        f"download {path}".encode("utf-8"),
        frozenset({FILE_MD5}),
        timeout_seconds,
    )
    advertised_md5 = _frame(responses, FILE_MD5).payload.decode("ascii").lower()
    responses = _exchange_until(
        client,
        FILE_GEOMETRY,
        b"",
        frozenset({FILE_GEOMETRY}),
        timeout_seconds,
    )
    geometry = _frame(responses, FILE_GEOMETRY).payload
    if len(geometry) != 6:
        raise FileTransferError("target returned invalid download geometry")
    block_count = int.from_bytes(geometry[:4], "big")
    block_size = int.from_bytes(geometry[4:], "big")
    if block_count == 0 or block_size == 0:
        raise FileTransferError("target returned empty download geometry")

    content = bytearray()
    for sequence in range(1, block_count + 1):
        responses = _exchange_until(
            client,
            FILE_DATA,
            sequence.to_bytes(4, "big"),
            frozenset({FILE_DATA}),
            timeout_seconds,
        )
        packet = _frame(responses, FILE_DATA).payload
        if len(packet) < 4 or int.from_bytes(packet[:4], "big") != sequence:
            raise FileTransferError(f"target returned invalid download block {sequence}")
        block = packet[4:]
        if len(block) > block_size:
            raise FileTransferError(f"target exceeded announced block size at {sequence}")
        content.extend(block)

    responses = _exchange_until(
        client,
        FILE_COMPLETE,
        b"",
        frozenset({FILE_COMPLETE}),
        timeout_seconds,
    )
    _frame(responses, FILE_COMPLETE)
    calculated_md5 = hashlib.md5(content).hexdigest()
    if verify_md5 and calculated_md5 != advertised_md5:
        raise FileTransferError(
            f"download MD5 mismatch: target={advertised_md5} host={calculated_md5}"
        )
    return bytes(content)
