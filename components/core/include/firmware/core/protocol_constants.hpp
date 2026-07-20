// Defines shared packet identifiers and wire limits without depending on services.
#pragma once

#include <cstddef>
#include <cstdint>

namespace firmware::core::protocol {

inline constexpr std::uint8_t controller_version = 0x71U;
inline constexpr std::uint8_t machine_status = 0x81U;
inline constexpr std::uint8_t diagnostic_data = 0x82U;
inline constexpr std::uint8_t text_response = 0x83U;
inline constexpr std::uint8_t operation_success = 0x84U;
inline constexpr std::uint8_t operation_failure = 0x85U;
inline constexpr std::uint8_t console_message = 0x90U;
inline constexpr std::uint8_t ownership_limit = 0x91U;
inline constexpr std::uint8_t single_command = 0xA1U;
inline constexpr std::uint8_t general_command = 0xA2U;
inline constexpr std::uint8_t file_command = 0xB0U;
inline constexpr std::uint8_t file_md5 = 0xB1U;
inline constexpr std::uint8_t file_geometry = 0xB2U;
inline constexpr std::uint8_t file_data = 0xB3U;
inline constexpr std::uint8_t file_complete = 0xB4U;
inline constexpr std::uint8_t file_cancel = 0xB5U;
inline constexpr std::uint8_t file_retry = 0xB6U;
inline constexpr std::uint8_t play_status = 0xB7U;
inline constexpr std::uint8_t firmware_family = 0xC0U;
inline constexpr std::uint8_t configuration_family = 0xD0U;
inline constexpr std::uint8_t factory_family = 0xE0U;
inline constexpr std::uint8_t play_family = 0xF0U;
inline constexpr std::uint8_t family_mask = 0xF0U;
inline constexpr std::uint8_t operation_mask = 0x0FU;
inline constexpr std::uint8_t transfer_start = 1U;
inline constexpr std::uint8_t transfer_geometry = 2U;
inline constexpr std::uint8_t transfer_data = 3U;
inline constexpr std::uint8_t transfer_complete = 4U;
inline constexpr std::uint8_t transfer_cancel = 5U;
inline constexpr std::uint8_t play_goto = 6U;
inline constexpr std::uint8_t play_progress = 7U;

inline constexpr std::size_t big_endian_u16_size = 2U;
inline constexpr std::size_t big_endian_u32_size = 4U;
inline constexpr std::size_t common_frame_overhead = 9U;
inline constexpr std::size_t controller_maximum_frame_size = 528U;
inline constexpr std::size_t controller_maximum_item_size = 544U;
inline constexpr std::size_t host_maximum_frame_size = 8300U;

// Combines a family high nibble and operation low nibble into one packet type.
constexpr std::uint8_t family_packet(std::uint8_t family, std::uint8_t operation) {
    return static_cast<std::uint8_t>((family & family_mask) | (operation & operation_mask));
}

}  // namespace firmware::core::protocol
