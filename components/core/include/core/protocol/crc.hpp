/** @file @brief Checksum algorithms used by framing, BLUFI, and updates. */
#pragma once
#include "core/protocol/bytes.hpp"
#include <cstdint>
namespace firmware::core {
/** Computes the common CCITT CRC-16 used by ordinary protocol frames. */
std::uint16_t crc16_ccitt(BytesView bytes);
/** Computes the controller update compatibility CRC required by FRM-007. */
std::uint16_t crc16_controller_update(BytesView bytes);
/** Computes the BLUFI wire CRC with its protocol-specific initialization. */
std::uint16_t crc16_blufi(BytesView bytes);
/** Computes the ISO-HDLC CRC-32 used by aggregate firmware packages. */
std::uint32_t crc32_iso_hdlc(BytesView bytes);
}  // namespace firmware::core
