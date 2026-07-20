// Defines deterministic host file-transfer path and MD5 cache transformations.
#pragma once

#include "firmware/core/bytes.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace firmware::core {

enum class FileTransferDirection {
    upload,
    download,
};

// Holds one validated start direction and normalized absolute path.
struct FileTransferStart {
    FileTransferDirection direction;
    std::string path;
};

// Holds optional MD5 and compressed sidecar paths for a logical file.
struct FileCachePaths {
    std::optional<std::string> md5_path;
    std::optional<std::string> compressed_path;
};

// Parses, decodes, trims, and resolves one `0xB0` start payload.
std::optional<FileTransferStart> parse_file_transfer_start(BytesView payload);

// Applies the first-literal-substring cache mapping to a resolved path.
FileCachePaths map_file_cache_paths(std::string_view resolved_path);

// Collects and normalizes the first 32 hexadecimal characters in 63 bytes.
std::optional<std::string> extract_cached_md5(BytesView content);

}  // namespace firmware::core
