// Declares checksum algorithms required by framing, BLUFI, and update packages.
#pragma once
#include "firmware/core/bytes.hpp"
#include <cstdint>
namespace firmware::core {
std::uint16_t crc16_ccitt(BytesView bytes);
std::uint16_t crc16_blufi(BytesView bytes);
std::uint32_t crc32_iso_hdlc(BytesView bytes);
}  // namespace firmware::core
