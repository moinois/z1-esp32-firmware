/** @file @brief Multipart boundary validation for web update endpoints. */
#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace firmware::core {

namespace web_update {

/// Maximum Content-Type header accepted before parsing multipart parameters.
inline constexpr std::size_t content_type_capacity = 512U;

}  // namespace web_update

/** Extracts a validated multipart boundary without copying the header.
 *  @return Boundary suffix, or no value when missing, empty, or oversized.
 */
std::optional<std::string_view> parse_multipart_content_type(
    std::string_view content_type);

}  // namespace firmware::core
