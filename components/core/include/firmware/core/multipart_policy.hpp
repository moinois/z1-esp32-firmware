// Declares multipart Content-Type boundary extraction for web update endpoints.
#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace firmware::core {

namespace web_update {

inline constexpr std::size_t content_type_capacity = 512U;

}  // namespace web_update

// Returns the boundary suffix or rejects a missing/empty/oversized header.
std::optional<std::string_view> parse_multipart_content_type(
    std::string_view content_type);

}  // namespace firmware::core
