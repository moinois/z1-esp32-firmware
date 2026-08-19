/** @file @brief Implements host file-transfer start parsing and cache path derivation. */
#include "core/filesystem/file_transfer_paths.hpp"

#include "core/filesystem/sd_user_path.hpp"
#include "core/filesystem/file_transfer_limits.hpp"
#include "core/protocol/text.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace firmware::core {
namespace {

constexpr std::size_t maximum_start_size = 128U;
constexpr std::size_t maximum_md5_input_size = 63U;

// Reports whether a trailing path byte is removed by HFT-003.
bool trailing_trimmed_character(char value) {
    return value == '\n' || value == '\r' || value == ' ';
}

// HFT-003 removes leading ASCII spaces but preserves tabs as path bytes.
std::string trim_path(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(),
                                        [](char byte) { return byte == ' '; });
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
                                       trailing_trimmed_character).base();
    if (first >= last) {
        return {};
    }
    return {first, last};
}

// Reports whether payload begins with the requested case-sensitive prefix.
bool begins_with(BytesView payload, std::string_view prefix) {
    return payload.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), payload.begin());
}

}  // namespace

std::optional<FileTransferStart> parse_file_transfer_start(BytesView payload) {
    if (payload.size() > maximum_start_size) {
        return std::nullopt;
    }

    FileTransferDirection direction;
    std::size_t path_offset = 0U;
    if (payload.size() > 7U && begins_with(payload, "upload")) {
        direction = FileTransferDirection::upload;
        path_offset = 7U;
    } else if (payload.size() > 9U && begins_with(payload, "download")) {
        direction = FileTransferDirection::download;
        path_offset = 9U;
    } else {
        return std::nullopt;
    }

    const std::string decoded = decode_escaped(
        {payload.data() + path_offset, payload.size() - path_offset});
    const std::string trimmed = trim_path(decoded);
    if (trimmed.empty() && direction == FileTransferDirection::upload) {
        return std::nullopt;
    }
    std::string resolved = normalize_path(trimmed);
    if (resolved.size() > file_transfer_limits::maximum_path_size) {
        return std::nullopt;
    }
    return FileTransferStart{direction, std::move(resolved)};
}

FileCachePaths map_file_cache_paths(std::string_view resolved_path) {
    constexpr std::string_view marker = "gcodes/";
    const std::size_t marker_position = resolved_path.find(marker);
    if (marker_position != std::string_view::npos) {
        const std::string relative(resolved_path.substr(marker_position + marker.size()));
        return {
            physical_sd_path("/gcodes/.md5/") + relative,
            physical_sd_path("/gcodes/.lz/") + relative,
        };
    }

    if (resolved_path.compare(0U, sd_mount_prefix.size(), sd_mount_prefix) == 0) {
        return {physical_sd_path("/.md5/") +
                    std::string(resolved_path.substr(sd_mount_prefix.size())),
                std::nullopt};
    }
    return {};
}

std::optional<std::string> extract_cached_md5(BytesView content) {
    std::string result;
    const std::size_t inspected = std::min(content.size(), maximum_md5_input_size);
    for (std::size_t index = 0U;
         index < inspected && result.size() < file_transfer_limits::md5_text_size;
         ++index) {
        const unsigned char character = content[index];
        if (std::isxdigit(character) != 0) {
            result.push_back(static_cast<char>(std::tolower(character)));
        }
    }
    if (result.size() != file_transfer_limits::md5_text_size) {
        return std::nullopt;
    }
    return result;
}

}  // namespace firmware::core
