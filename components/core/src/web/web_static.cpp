/** @file @brief Implements literal static paths, MIME selection, and firmware identity. */
#include "core/web/web_static.hpp"

namespace firmware::core {
namespace {

constexpr std::string_view html_marker = ".html";
constexpr std::string_view css_marker = "css";
constexpr std::string_view javascript_marker = ".js";
constexpr std::string_view html_mime_type = "text/html";
constexpr std::string_view css_mime_type = "text/css";
constexpr std::string_view javascript_mime_type = "application/javascript";
constexpr std::string_view plain_text_mime_type = "text/plain";
constexpr std::string_view fixed_firmware_identity =
    "{\"version\":\"0.1.11\",\"build_date\":\"2026.06.22\","
    "\"idf_ver\":\"v5.4.1\"}";

// Reports whether the complete path names a directory resource.
bool ends_in_directory_separator(std::string_view path) {
    return !path.empty() && path.back() == '/';
}

}  // namespace

std::optional<std::string> resolve_static_path(std::string_view request_uri) {
    std::string path(web::volume_prefix);
    path.append(request_uri);
    if (ends_in_directory_separator(path)) {
        path.append(web::directory_index_name);
    }
    if (path.size() >= web::static_path_capacity) {
        return std::nullopt;
    }
    return path;
}

std::string_view static_mime_type(std::string_view path) {
    if (path.find(html_marker) != std::string_view::npos) {
        return html_mime_type;
    }
    if (path.find(css_marker) != std::string_view::npos) {
        return css_mime_type;
    }
    if (path.find(javascript_marker) != std::string_view::npos) {
        return javascript_mime_type;
    }
    return plain_text_mime_type;
}

std::string_view firmware_identity_json() {
    return fixed_firmware_identity;
}

}  // namespace firmware::core
