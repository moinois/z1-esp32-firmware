/** @file @brief Side-effect-free parsing of aggregate firmware packages. */
#pragma once
#include "core/protocol/bytes.hpp"
#include "core/protocol/crc.hpp"
#include <cstdint>
#include <optional>
namespace firmware::core {

/// Fixed encoded header length before optional image payloads.
inline constexpr std::size_t update_package_header_size = 32U;

/** Decoded metadata that locates and versions both optional image payloads. */
struct UpdateHeader {
    /// Aggregate container format version, not a product firmware version.
    std::uint8_t version = 0;
    /// Presence and policy bits validated by the parser.
    std::uint8_t flags = 0;
    /// Mainboard payload length in bytes; zero means absent.
    std::uint32_t mainboard_size = 0;
    /// Controller payload length in bytes; zero means absent.
    std::uint32_t controller_size = 0;
    /// Human/product version metadata for the mainboard image.
    std::uint32_t mainboard_version = 0;
    /// Human/product version metadata for the controller image.
    std::uint32_t controller_version = 0;
};

/** First validation failure observed while parsing an aggregate package. */
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

/** Parsed metadata paired with an explicit validation outcome. */
struct UpdateParseResult {
    std::optional<UpdateHeader> header;
    UpdateError error = UpdateError::none;

    /// Reports whether metadata exists and every validation stage succeeded.
    bool valid() const {
        return header.has_value() && error == UpdateError::none;
    }
};
/** Computes the aggregate CRC while excluding the encoded CRC field itself. */
std::uint32_t aggregate_file_crc(BytesView package);
/** Validates and decodes a complete in-memory aggregate firmware package. */
UpdateParseResult parse_update_package(BytesView package);
}  // namespace firmware::core
