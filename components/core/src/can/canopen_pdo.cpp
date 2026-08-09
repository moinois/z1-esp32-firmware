/** @file @brief Implements deterministic enabled-RPDO mapping and dictionary writes. */
#include "core/can/canopen_pdo.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace firmware::core {
namespace {

constexpr std::uint8_t mapping_count_subindex = 0U;
constexpr std::uint8_t communication_identifier_subindex = 1U;
constexpr std::uint8_t mapping_first_subindex = 1U;
constexpr std::uint32_t identifier_mask = 0x7ffU;

std::uint32_t decode_le(const std::array<std::uint8_t, 8U>& data,
                        std::size_t offset, std::size_t size) {
    std::uint32_t value = 0U;
    for (std::size_t index = 0U; index < size; ++index) {
        value |= static_cast<std::uint32_t>(data[offset + index])
                 << (index * 8U);
    }
    return value;
}

}  // namespace

CanopenReceivePdoRouter::CanopenReceivePdoRouter(
    CanopenObjectDictionary& dictionary)
    : dictionary_(dictionary) {}

bool CanopenReceivePdoRouter::receive(const CanFrame& frame) {
    for (std::uint8_t pdo = 0U; pdo < canopen_dictionary::pdo_count; ++pdo) {
        if (receive_from(pdo, frame)) {
            return true;
        }
    }
    return false;
}

bool CanopenReceivePdoRouter::receive_from(std::uint8_t pdo_number,
                                           const CanFrame& frame) {
    const auto identifier = dictionary_.read(
        static_cast<std::uint16_t>(canopen_object::first_rpdo_communication +
                                   pdo_number),
        communication_identifier_subindex);
    if (identifier.abort != SdoAbort::none || identifier.data.size() != 4U) {
        return false;
    }
    const std::uint32_t configured_identifier =
        static_cast<std::uint32_t>(identifier.data[0]) |
        (static_cast<std::uint32_t>(identifier.data[1]) << 8U) |
        (static_cast<std::uint32_t>(identifier.data[2]) << 16U) |
        (static_cast<std::uint32_t>(identifier.data[3]) << 24U);
    if ((configured_identifier & canopen_dictionary::disabled_identifier_mask) != 0U ||
        (configured_identifier & identifier_mask) != frame.identifier) {
        return false;
    }

    const auto count = dictionary_.read(
        static_cast<std::uint16_t>(canopen_object::first_rpdo_mapping +
                                   pdo_number),
        mapping_count_subindex);
    if (count.abort != SdoAbort::none || count.data.size() != 1U ||
        count.data[0] > canopen_dictionary::maximum_mapping_entries) {
        return false;
    }
    std::size_t payload_offset = 0U;
    for (std::uint8_t entry = 0U; entry < count.data[0]; ++entry) {
        const auto mapping = dictionary_.read(
            static_cast<std::uint16_t>(canopen_object::first_rpdo_mapping +
                                       pdo_number),
            static_cast<std::uint8_t>(mapping_first_subindex + entry));
        if (mapping.abort != SdoAbort::none || mapping.data.size() != 4U) {
            return false;
        }
        const std::uint32_t descriptor =
            static_cast<std::uint32_t>(mapping.data[0]) |
            (static_cast<std::uint32_t>(mapping.data[1]) << 8U) |
            (static_cast<std::uint32_t>(mapping.data[2]) << 16U) |
            (static_cast<std::uint32_t>(mapping.data[3]) << 24U);
        const std::size_t width_bits = descriptor & 0xffU;
        if (width_bits == 0U || width_bits % 8U != 0U ||
            width_bits > 32U || payload_offset + width_bits / 8U > frame.size) {
            return false;
        }
        const std::uint16_t index = static_cast<std::uint16_t>(descriptor >> 16U);
        const std::uint8_t subindex = static_cast<std::uint8_t>(descriptor >> 8U);
        const std::size_t width_bytes = width_bits / 8U;
        const std::uint32_t value = decode_le(frame.data, payload_offset,
                                              width_bytes);
        ByteVector encoded(width_bytes);
        for (std::size_t byte = 0U; byte < width_bytes; ++byte) {
            encoded[byte] = static_cast<std::uint8_t>(value >> (byte * 8U));
        }
        if (dictionary_.write(index, subindex, encoded).abort != SdoAbort::none) {
            return false;
        }
        payload_offset += width_bytes;
    }
    return payload_offset == frame.size;
}

}  // namespace firmware::core
