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

/** Extracts the exact multipart boundary suffix without copying the header.
 *  @return Boundary suffix, including quotes or emptiness, or no value when the
 *          marker is missing or the stored header is oversized.
 */
std::optional<std::string_view> parse_multipart_content_type(
    std::string_view content_type);

}  // namespace firmware::core
