// Implements overflow-safe aggregate header and checksum validation.
#include "firmware/core/update_package.hpp"

namespace firmware::core {
namespace {

std::uint32_t read_le32(const ByteVector& bytes, std::size_t offset) {
    return bytes[offset] | (std::uint32_t(bytes[offset + 1]) << 8U) |
           (std::uint32_t(bytes[offset + 2]) << 16U) | (std::uint32_t(bytes[offset + 3]) << 24U);
}

}  // namespace

std::uint32_t aggregate_file_crc(const ByteVector& package) {
    if (package.size() < 32U) {
        return 0;
    }

    std::uint32_t crc = 0xFFFFFFFFUL;
    const auto add_byte = [&crc](std::uint8_t byte) {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) ? (crc >> 1U) ^ 0xEDB88320UL : crc >> 1U;
        }
    };
    for (std::size_t index = 0; index < 28U; ++index) {
        add_byte(package[index]);
    }
    for (std::size_t index = 32U; index < package.size(); ++index) {
        add_byte(package[index]);
    }
    return crc ^ 0xFFFFFFFFUL;
}

UpdateParseResult parse_update_package(const ByteVector& package) {
    if (package.size() < 32U) {
        return {{}, UpdateError::short_file};
    }
    if (read_le32(package, 0) != 0x4D5173EEUL) {
        return {{}, UpdateError::magic};
    }
    if (package[4] != 1U && package[4] != 2U) {
        return {{}, UpdateError::version};
    }
    if (package[5] != 32U) {
        return {{}, UpdateError::header_length};
    }

    const auto mainboard_size = read_le32(package, 8);
    const auto controller_size = read_le32(package, 12);
    const auto expected_flags = static_cast<std::uint8_t>((mainboard_size ? 1U : 0U) | (controller_size ? 2U : 0U));
    if ((package[6] & 3U) != expected_flags) {
        return {{}, UpdateError::flags};
    }
    if (crc32_iso_hdlc({package.data(), 24}) != read_le32(package, 24)) {
        return {{}, UpdateError::header_crc};
    }
    if (aggregate_file_crc(package) != read_le32(package, 28)) {
        return {{}, UpdateError::file_crc};
    }
    if (32ULL + mainboard_size + controller_size > package.size()) {
        return {{}, UpdateError::size};
    }
    if (mainboard_size != 0U && package[32] != 0xE9U) {
        return {{}, UpdateError::image};
    }

    return {UpdateHeader{package[4], package[6], mainboard_size, controller_size,
                         read_le32(package, 16), read_le32(package, 20)},
            UpdateError::none};
}

}  // namespace firmware::core
