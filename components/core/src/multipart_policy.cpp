// Implements bounded multipart Content-Type boundary extraction.
#include "firmware/core/multipart_policy.hpp"

namespace firmware::core {

std::optional<std::string_view> parse_multipart_content_type(
    std::string_view content_type) {
    if (content_type.empty() ||
        content_type.size() > web_update::content_type_capacity) {
        return std::nullopt;
    }
    constexpr std::string_view boundary_marker = "boundary=";
    const std::size_t marker_start = content_type.find(boundary_marker);
    if (marker_start == std::string_view::npos) {
        return std::nullopt;
    }
    std::string_view boundary = content_type.substr(
        marker_start + boundary_marker.size());
    if (boundary.empty()) {
        return std::nullopt;
    }
    if (boundary.size() >= 2U && boundary.front() == '"' &&
        boundary.back() == '"') {
        boundary.remove_prefix(1U);
        boundary.remove_suffix(1U);
    }
    if (boundary.empty()) {
        return std::nullopt;
    }
    return boundary;
}

}  // namespace firmware::core
