"""Encodes and decodes the firmware's small binary host protocol.

The provisioning command is intentionally kept independent from USB and TCP so
the same frame can be transported over either connection.
"""

from __future__ import annotations

from typing import List, Tuple

GENERAL_COMMAND = 0xA2
TEXT_RESPONSE = 0x83
OPERATION_SUCCESS = 0x84
OPERATION_FAILURE = 0x85

_SYNC = b"\x86\x68"
_TAIL = b"\x55\xAA"
_MINIMUM_DATA_LENGTH = 3
_MAXIMUM_DATA_LENGTH = 0xFFFF


def _crc16_ccitt(data: bytes) -> int:
    """Returns the zero-initialized CRC-16/CCITT used by firmware frames."""
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def _escape_token(value: str) -> bytes:
    """Encodes token characters understood by firmware's escape decoder."""
    replacements = {
        " ": b"\x01",
        "?": b"\x02",
        "*": b"\x03",
        "!": b"\x04",
        "~": b"\x05",
    }
    encoded = bytearray()
    for character in value:
        if character == "\x00":
            raise ValueError("SSID and password cannot contain NUL")
        encoded.extend(replacements.get(character, character.encode("utf-8")))
    return bytes(encoded)


def encode_frame(frame_type: int, payload: bytes) -> bytes:
    """Builds one sync, length, payload, CRC, and tail protocol frame."""
    data_length = len(payload) + 3
    if data_length > _MAXIMUM_DATA_LENGTH:
        raise ValueError("payload is too large for a firmware frame")
    body = bytes((frame_type,)) + payload
    length = data_length.to_bytes(2, "big")
    crc = _crc16_ccitt(length + body)
    return _SYNC + length + body + crc.to_bytes(2, "big") + _TAIL


def build_wifi_command(ssid: str, password: str) -> bytes:
    """Builds the local WLAN command that persists station credentials."""
    command = b"wlan -s " + _escape_token(ssid) + b" " + _escape_token(password)
    return encode_frame(GENERAL_COMMAND, command)


def decode_frames(data: bytes) -> Tuple[List[Tuple[int, bytes]], bytes]:
    """Decodes complete response frames and returns an incomplete remainder."""
    frames: List[Tuple[int, bytes]] = []
    buffer = bytearray(data)
    while True:
        sync_index = buffer.find(_SYNC)
        if sync_index < 0:
            return frames, bytes(buffer[-1:] if buffer.endswith(_SYNC[:1]) else b"")
        if sync_index:
            del buffer[:sync_index]
        if len(buffer) < 4:
            return frames, bytes(buffer)
        data_length = int.from_bytes(buffer[2:4], "big")
        if data_length < _MINIMUM_DATA_LENGTH:
            del buffer[:2]
            continue
        total_size = data_length + 6
        if len(buffer) < total_size:
            return frames, bytes(buffer)
        candidate = bytes(buffer[:total_size])
        crc_offset = total_size - 4
        expected_crc = int.from_bytes(
            candidate[crc_offset : crc_offset + 2], "big"
        )
        actual_crc = _crc16_ccitt(candidate[2:crc_offset])
        if candidate[-2:] != _TAIL or expected_crc != actual_crc:
            del buffer[:1]
            continue
        frames.append((candidate[4], candidate[5:crc_offset]))
        del buffer[:total_size]
