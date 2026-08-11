/** @file @brief Declares exact DIAG-025 BLUFI custom-data formatting. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace firmware::application {

/// Formats the mandatory APP_BLUFI record using the complete received length.
std::string blufi_custom_data_length_message(std::size_t data_size);

/** Formats one bounded Custom Data record starting at `offset`.
 *
 * Offsets at or beyond the normative 65535-byte diagnostic limit return no
 * record. Each successful call formats at most 16 bytes, allowing the target
 * adapter to emit incrementally instead of retaining thousands of strings.
 */
std::optional<std::string> blufi_custom_data_hex_message(
    core::BytesView data, std::size_t offset);

}  // namespace firmware::application
