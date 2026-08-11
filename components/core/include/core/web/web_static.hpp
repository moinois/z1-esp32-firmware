/** @file @brief Static-file resolution and immutable firmware web identity. */
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::core {

namespace web {

/// Private VFS prefix containing browser assets.
inline constexpr std::string_view volume_prefix = "/spiffs";
/// File appended when a request targets a directory path.
inline constexpr std::string_view directory_index_name = "index.html";
/// Maximum resolved VFS path including its terminator budget.
inline constexpr std::size_t static_path_capacity = 256U;
/// Streaming read size chosen to bound HTTP worker memory.
inline constexpr std::size_t static_file_chunk_size = 256U;
/// Stable response body used when no static resource exists.
inline constexpr std::string_view missing_static_file_body =
    "Nothing matches the given URI";

}  // namespace web

/** Resolves a request URI beneath SPIFFS, truncating to its fixed C buffer. */
std::optional<std::string> resolve_static_path(std::string_view request_uri);

/** Selects a MIME type using the required case-sensitive suffix precedence. */
std::string_view static_mime_type(std::string_view path);

/** Returns the immutable public firmware identity JSON document. */
std::string_view firmware_identity_json();

}  // namespace firmware::core
