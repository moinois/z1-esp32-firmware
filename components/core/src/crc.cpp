// Implements compact table-free checksums to avoid permanent lookup-table cost.
#include "firmware/core/crc.hpp"
namespace firmware::core {
namespace {
std::uint16_t crc16(BytesView bytes, std::uint16_t initial) {
    std::uint16_t crc = initial;
    for (const auto byte : bytes) {
        crc ^= static_cast<std::uint16_t>(byte) << 8U;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = static_cast<std::uint16_t>((crc & 0x8000U) ? (crc << 1U) ^ 0x1021U : crc << 1U);
        }
    }
    return crc;
}
}  // namespace
std::uint16_t crc16_ccitt(BytesView bytes) {
    return crc16(bytes, 0);
}

std::uint16_t crc16_blufi(BytesView bytes) {
    return static_cast<std::uint16_t>(crc16(bytes, 0xFFFFU) ^ 0xFFFFU);
}

std::uint32_t crc32_iso_hdlc(BytesView bytes) {
    std::uint32_t crc = 0xFFFFFFFFUL;
    for (const auto byte : bytes) {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) ? (crc >> 1U) ^ 0xEDB88320UL : crc >> 1U;
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}
}  // namespace firmware::core
