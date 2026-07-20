// Defines side-effect-free parsing of aggregate SD firmware packages.
#pragma once
#include "firmware/core/bytes.hpp"
#include "firmware/core/crc.hpp"
#include <cstdint>
#include <optional>
namespace firmware::core {
struct UpdateHeader {
    std::uint8_t version = 0;
    std::uint8_t flags = 0;
    std::uint32_t mainboard_size = 0;
    std::uint32_t controller_size = 0;
    std::uint32_t mainboard_version = 0;
    std::uint32_t controller_version = 0;
};

enum class UpdateError {
    none,
    short_file,
    magic,
    version,
    header_length,
    flags,
    header_crc,
    file_crc,
    size,
    image
};

struct UpdateParseResult {
    std::optional<UpdateHeader> header;
    UpdateError error = UpdateError::none;

    bool valid() const {
        return header.has_value() && error == UpdateError::none;
    }
};
std::uint32_t aggregate_file_crc(const ByteVector& package);
UpdateParseResult parse_update_package(const ByteVector& package);
}  // namespace firmware::core
