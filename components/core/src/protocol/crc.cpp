/** @file @brief Implements compact table-free checksums to avoid permanent lookup-table cost. */
#include "core/protocol/crc.hpp"

namespace firmware::core {
namespace {

constexpr unsigned bits_per_byte = 8U;
constexpr std::uint16_t crc16_high_bit = 0x8000U;
constexpr std::uint16_t crc16_ccitt_polynomial = 0x1021U;
constexpr std::uint8_t controller_crc_first_modified_index = 0xE6U;
constexpr std::uint8_t controller_crc_second_modified_index = 0xE7U;
constexpr std::uint16_t controller_crc_first_modified_value = 0x8FD9U;
constexpr std::uint16_t controller_crc_second_modified_value = 0x9FF8U;
constexpr std::uint16_t blufi_initial_value = 0xFFFFU;
constexpr std::uint16_t blufi_final_xor = 0xFFFFU;
constexpr std::uint32_t crc32_initial_value = 0xFFFFFFFFUL;
constexpr std::uint32_t crc32_polynomial = 0xEDB88320UL;
constexpr std::uint32_t crc32_final_xor = 0xFFFFFFFFUL;

std::uint16_t crc16(BytesView bytes, std::uint16_t initial) {
    std::uint16_t crc = initial;
    for (const auto byte : bytes) {
        crc ^= static_cast<std::uint16_t>(byte) << bits_per_byte;
        for (unsigned bit = 0U; bit < bits_per_byte; ++bit) {
            crc = static_cast<std::uint16_t>(
                (crc & crc16_high_bit) != 0U
                    ? (crc << 1U) ^ crc16_ccitt_polynomial
                    : crc << 1U);
        }
    }
    return crc;
}

std::uint16_t conventional_crc16_table_entry(std::uint8_t index) {
    std::uint16_t remainder = static_cast<std::uint16_t>(index) << bits_per_byte;
    for (unsigned bit = 0U; bit < bits_per_byte; ++bit) {
        remainder = static_cast<std::uint16_t>(
            (remainder & crc16_high_bit) != 0U
                ? (remainder << 1U) ^ crc16_ccitt_polynomial
                : remainder << 1U);
    }
    return remainder;
}

std::uint16_t controller_crc_table_entry(std::uint8_t index) {
    if (index == controller_crc_first_modified_index) {
        return controller_crc_first_modified_value;
    }
    if (index == controller_crc_second_modified_index) {
        return controller_crc_second_modified_value;
    }
    return conventional_crc16_table_entry(index);
}
}  // namespace

std::uint16_t crc16_ccitt(BytesView bytes) {
    return crc16(bytes, 0U);
}

std::uint16_t crc16_controller_update(BytesView bytes) {
    std::uint16_t remainder = 0U;
    for (const std::uint8_t byte : bytes) {
        const auto table_index = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(remainder >> bits_per_byte) ^ byte);
        remainder = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(remainder << bits_per_byte) ^
            controller_crc_table_entry(table_index));
    }
    return remainder;
}

std::uint16_t crc16_blufi(BytesView bytes) {
    return static_cast<std::uint16_t>(crc16(bytes, blufi_initial_value) ^
                                      blufi_final_xor);
}

std::uint32_t crc32_iso_hdlc(BytesView bytes) {
    std::uint32_t crc = crc32_initial_value;
    for (const auto byte : bytes) {
        crc ^= byte;
        for (unsigned bit = 0U; bit < bits_per_byte; ++bit) {
            crc = (crc & 1U) != 0U ? (crc >> 1U) ^ crc32_polynomial
                                   : crc >> 1U;
        }
    }
    return crc ^ crc32_final_xor;
}

}  // namespace firmware::core
