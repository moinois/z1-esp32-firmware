"""Runs complete host file transfers through a target's public protocol."""

from __future__ import annotations

import hashlib
from typing import List, Protocol

from tests.hardware.hil_protocol import ReceivedFrame

FILE_COMMAND = 0xB0
FILE_MD5 = 0xB1
FILE_GEOMETRY = 0xB2
FILE_DATA = 0xB3
FILE_COMPLETE = 0xB4
FILE_CANCEL = 0xB5
FILE_RETRY = 0xB6
FILE_SUCCESS = 0x90


class FileTransferClient(Protocol):
    """Describes the transport operation required by the transfer driver."""

    def send(self, frame_type: int, payload: bytes) -> None:
        """Sends one framed packet without waiting for a target response."""

        ...

    def exchange(
        self, frame_type: int, payload: bytes, timeout_seconds: float = 3.0
    ) -> List[ReceivedFrame]:
        """Sends one framed phase packet and returns decoded target responses."""

        ...

    def receive(
        self,
        timeout_seconds: float = 3.0,
        *,
        terminal_types: frozenset[int] | None = None,
    ) -> List[ReceivedFrame]:
        """Receives target frames without sending another host packet."""

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
    digest = hashlib.md5(data).hexdigest().encode("ascii")
    # HFTU-003 makes upload admission silent. The first transfer packet is the
    # host's MD5; waiting for a target-side B1 prompt here would deadlock a
    # conforming implementation.
    client.send(FILE_COMMAND, command)
    responses = _exchange_until(
        client, FILE_MD5, digest, frozenset({FILE_GEOMETRY}), timeout_seconds
    )
    if not any(
        response.frame_type in {FILE_GEOMETRY, FILE_RETRY, FILE_CANCEL}
        for response in responses
    ):
        # If TRN-006 omitted the B2 acknowledgement, HFTU-024 requires 51
        # unmatched packets before repeating the currently required request.
        # Repeat MD5 at a pace that lets the latest-value worker process every
        # packet; never duplicate the already admitted upload start.
        for _ in range(51):
            client.send(FILE_MD5, digest)
            responses = client.receive(
                min(timeout_seconds, 0.05),
                terminal_types=frozenset(
                    {FILE_GEOMETRY, FILE_CANCEL, FILE_RETRY}
                ),
            )
            if any(
                frame.frame_type in {FILE_GEOMETRY, FILE_CANCEL, FILE_RETRY}
                for frame in responses
            ):
                break
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

    # B4 acknowledges the final data write, while HFTU-009 releases ownership
    # and then emits 0x90. Waiting for that protocol event prevents the next
    # operation from racing finalization or consuming the delayed success frame.
    released = client.receive(
        timeout_seconds,
        terminal_types=frozenset({FILE_SUCCESS, FILE_CANCEL}),
    )
    _frame(released, FILE_SUCCESS)


def download_file(
    client: FileTransferClient,
    path: str,
    *,
    timeout_seconds: float = 5.0,
    verify_md5: bool = True,
) -> bytes:
    """Downloads every announced block and verifies its advertised MD5."""

    command = f"download {path}".encode("utf-8")
    responses = _exchange_until(
        client,
        FILE_COMMAND,
        command,
        frozenset({FILE_MD5}),
        timeout_seconds,
    )
    for _ in range(4):
        if any(
            frame.frame_type in {FILE_MD5, FILE_CANCEL} for frame in responses
        ):
            break
        # The initial MD5 is ordinary host output and may be omitted under
        # TRN-006. HFTD-005 defines B1 as an idempotent request for it.
        responses = _exchange_until(
            client,
            FILE_MD5,
            b"",
            frozenset({FILE_MD5}),
            timeout_seconds,
        )
        if any(
            frame.frame_type in {FILE_MD5, FILE_CANCEL} for frame in responses
        ):
            break
        # If the start itself never reached its bounded worker, retry admission
        # only after the phase-specific B1 probe established no active download.
        responses = _exchange_until(
            client,
            FILE_COMMAND,
            command,
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
    for _ in range(4):
        if any(
            frame.frame_type in {FILE_GEOMETRY, FILE_CANCEL}
            for frame in responses
        ):
            break
        # Geometry is also ordinary host output under TRN-006. HFTD-005 makes
        # the empty B2 request safely repeat the recalculated geometry.
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
        for _ in range(4):
            if any(
                frame.frame_type in {FILE_DATA, FILE_CANCEL}
                for frame in responses
            ):
                break
            # TRN-005 permits a data response to be omitted under output
            # pressure. HFTD-008 makes B6 the public recovery operation and
            # requires the target to reread the last requested sequence.
            responses = _exchange_until(
                client,
                FILE_RETRY,
                b"",
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
