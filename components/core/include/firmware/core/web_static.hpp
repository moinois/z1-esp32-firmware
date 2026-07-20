// Declares side-effect-free static-file and firmware-identity web policy.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::core {

namespace web {

inline constexpr std::string_view volume_prefix = "/spiffs";
inline constexpr std::string_view directory_index_name = "index.html";
inline constexpr std::size_t static_path_capacity = 256U;
inline constexpr std::size_t static_file_chunk_size = 256U;
inline constexpr std::string_view missing_static_file_body =
    "Nothing matches the given URI";

}  // namespace web

// Builds the literal SPIFFS path or rejects a result that cannot fit.
std::optional<std::string> resolve_static_path(std::string_view request_uri);

// Selects a response MIME type using the required case-sensitive precedence.
std::string_view static_mime_type(std::string_view path);

// Returns the immutable public firmware identity JSON document.
std::string_view firmware_identity_json();

}  // namespace firmware::core
