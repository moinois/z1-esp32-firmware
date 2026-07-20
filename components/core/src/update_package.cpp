// Implements overflow-safe aggregate header and checksum validation.
#include "firmware/core/update_package.hpp"

namespace firmware::core {
namespace {

constexpr std::size_t update_header_size = 32U;
constexpr std::size_t aggregate_crc_field_offset = 28U;
constexpr std::size_t version_offset = 4U;
constexpr std::size_t header_length_offset = 5U;
constexpr std::size_t flags_offset = 6U;
constexpr std::size_t mainboard_size_offset = 8U;
constexpr std::size_t controller_size_offset = 12U;
constexpr std::size_t mainboard_version_offset = 16U;
constexpr std::size_t controller_version_offset = 20U;
constexpr std::size_t header_crc_offset = 24U;
constexpr std::size_t file_crc_offset = 28U;
constexpr std::uint32_t package_magic = 0x4D5173EEUL;
constexpr std::uint8_t minimum_package_version = 1U;
constexpr std::uint8_t maximum_package_version = 2U;
constexpr std::uint8_t mainboard_present_flag = 1U;
constexpr std::uint8_t controller_present_flag = 2U;
constexpr std::uint8_t component_flags_mask =
    mainboard_present_flag | controller_present_flag;
constexpr std::uint8_t esp_image_magic = 0xE9U;
constexpr unsigned bits_per_byte = 8U;
constexpr std::uint32_t crc32_initial_value = 0xFFFFFFFFUL;
constexpr std::uint32_t crc32_polynomial = 0xEDB88320UL;
constexpr std::uint32_t crc32_final_xor = 0xFFFFFFFFUL;

std::uint32_t read_le32(const ByteVector& bytes, std::size_t offset) {
    return bytes[offset] | (std::uint32_t(bytes[offset + 1]) << 8U) |
           (std::uint32_t(bytes[offset + 2]) << 16U) | (std::uint32_t(bytes[offset + 3]) << 24U);
}

}  // namespace

std::uint32_t aggregate_file_crc(const ByteVector& package) {
    if (package.size() < update_header_size) {
        return 0U;
    }

    std::uint32_t crc = crc32_initial_value;
    const auto add_byte = [&crc](std::uint8_t byte) {
        crc ^= byte;
        for (unsigned bit = 0U; bit < bits_per_byte; ++bit) {
            crc = (crc & 1U) != 0U ? (crc >> 1U) ^ crc32_polynomial
                                   : crc >> 1U;
        }
    };
    for (std::size_t index = 0U; index < aggregate_crc_field_offset; ++index) {
        add_byte(package[index]);
    }
    for (std::size_t index = update_header_size; index < package.size(); ++index) {
        add_byte(package[index]);
    }
    return crc ^ crc32_final_xor;
}

UpdateParseResult parse_update_package(const ByteVector& package) {
    if (package.size() < update_header_size) {
        return {{}, UpdateError::short_file};
    }
    if (read_le32(package, 0U) != package_magic) {
        return {{}, UpdateError::magic};
    }
    const std::uint8_t package_version = package[version_offset];
    if (package_version < minimum_package_version ||
        package_version > maximum_package_version) {
        return {{}, UpdateError::version};
    }
    if (package[header_length_offset] != update_header_size) {
        return {{}, UpdateError::header_length};
    }

    const auto mainboard_size = read_le32(package, mainboard_size_offset);
    const auto controller_size = read_le32(package, controller_size_offset);
    const auto expected_flags = static_cast<std::uint8_t>(
        (mainboard_size != 0U ? mainboard_present_flag : 0U) |
        (controller_size != 0U ? controller_present_flag : 0U));
    if ((package[flags_offset] & component_flags_mask) != expected_flags) {
        return {{}, UpdateError::flags};
    }
    if (crc32_iso_hdlc({package.data(), header_crc_offset}) !=
        read_le32(package, header_crc_offset)) {
        return {{}, UpdateError::header_crc};
    }
    if (aggregate_file_crc(package) != read_le32(package, file_crc_offset)) {
        return {{}, UpdateError::file_crc};
    }
    if (static_cast<std::uint64_t>(update_header_size) + mainboard_size + controller_size >
        package.size()) {
        return {{}, UpdateError::size};
    }
    if (mainboard_size != 0U && package[update_header_size] != esp_image_magic) {
        return {{}, UpdateError::image};
    }

    return {UpdateHeader{package_version, package[flags_offset], mainboard_size,
                         controller_size, read_le32(package, mainboard_version_offset),
                         read_le32(package, controller_version_offset)},
            UpdateError::none};
}

}  // namespace firmware::core
