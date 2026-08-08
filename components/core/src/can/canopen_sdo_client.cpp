/** @file @brief Implements exact little-endian expedited SDO client wire codecs. */
#include "firmware/core/canopen_sdo_client.hpp"

namespace firmware::core {
namespace {

constexpr std::uint8_t upload_command = 0x40U;
constexpr std::uint8_t download_32_command = 0x23U;
constexpr std::uint8_t upload_32_response = 0x43U;
constexpr std::uint8_t abort_command = 0x80U;

CanFrame base_request(std::uint8_t node, std::uint16_t index,
                      std::uint8_t subindex, std::uint8_t command) {
    CanFrame frame;
    frame.identifier = static_cast<std::uint16_t>(0x600U + node);
    frame.size = 8U;
    frame.data[0] = command;
    frame.data[1] = static_cast<std::uint8_t>(index);
    frame.data[2] = static_cast<std::uint8_t>(index >> 8U);
    frame.data[3] = subindex;
    return frame;
}

std::uint32_t decode_u32(const std::array<std::uint8_t, 8U>& data) {
    return static_cast<std::uint32_t>(data[4]) |
           (static_cast<std::uint32_t>(data[5]) << 8U) |
           (static_cast<std::uint32_t>(data[6]) << 16U) |
           (static_cast<std::uint32_t>(data[7]) << 24U);
}

}  // namespace

CanFrame make_sdo_upload_request(std::uint8_t node, std::uint16_t index,
                                 std::uint8_t subindex) {
    return base_request(node, index, subindex, upload_command);
}

CanFrame make_sdo_download_request(std::uint8_t node, std::uint16_t index,
                                   std::uint8_t subindex, std::uint32_t value) {
    CanFrame frame = base_request(node, index, subindex, download_32_command);
    frame.data[4] = static_cast<std::uint8_t>(value);
    frame.data[5] = static_cast<std::uint8_t>(value >> 8U);
    frame.data[6] = static_cast<std::uint8_t>(value >> 16U);
    frame.data[7] = static_cast<std::uint8_t>(value >> 24U);
    return frame;
}

std::optional<SdoClientResponse> parse_sdo_client_response(
    const CanFrame& frame, std::uint8_t node, std::uint16_t index,
    std::uint8_t subindex) {
    if (frame.identifier != static_cast<std::uint16_t>(0x580U + node) ||
        frame.size != 8U || frame.data[1] != static_cast<std::uint8_t>(index) ||
        frame.data[2] != static_cast<std::uint8_t>(index >> 8U) ||
        frame.data[3] != subindex) {
        return std::nullopt;
    }
    if (frame.data[0] == abort_command) {
        return SdoClientResponse{index, subindex, 0U, true};
    }
    if (frame.data[0] != upload_32_response) return std::nullopt;
    return SdoClientResponse{index, subindex, decode_u32(frame.data), false};
}

}  // namespace firmware::core
