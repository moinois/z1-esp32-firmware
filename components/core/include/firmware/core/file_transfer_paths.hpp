/** @file @brief Host transfer path parsing and sidecar transformations. */
#pragma once

#include "firmware/core/bytes.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace firmware::core {

/** Direction selected by the initiating `0xB0` host packet. */
enum class FileTransferDirection {
    /// Host writes a file to the device.
    upload,
    /// Host reads a file from the device.
    download,
};

/** Validated transfer direction and normalized logical SD path. */
struct FileTransferStart {
    FileTransferDirection direction;
    std::string path;
};

/** Derived cache paths; absence means the mapping rule does not apply. */
struct FileCachePaths {
    std::optional<std::string> md5_path;
    std::optional<std::string> compressed_path;
};

/** Parses, decodes, trims, and resolves one `0xB0` start payload. */
std::optional<FileTransferStart> parse_file_transfer_start(BytesView payload);

/** Applies token-aware `gcodes` and general SD cache mapping rules. */
FileCachePaths map_file_cache_paths(std::string_view resolved_path);

/** Collects and normalizes the first MD5 digest in the bounded cache payload. */
std::optional<std::string> extract_cached_md5(BytesView content);

}  // namespace firmware::core
