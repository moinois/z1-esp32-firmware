// Implements host file-transfer start parsing and cache path derivation.
#include "firmware/core/file_transfer_paths.hpp"

#include "firmware/core/text.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace firmware::core {
namespace {

constexpr std::size_t maximum_start_size = 128U;
constexpr std::size_t maximum_path_size = 255U;
constexpr std::size_t maximum_md5_input_size = 63U;
constexpr std::size_t md5_text_size = 32U;

// Reports whether a path edge character is trimmed by the transfer protocol.
bool trimmed_character(char value) {
    return value == '\t' || value == '\n' || value == '\r' || value == ' ';
}

// Removes protocol-defined whitespace from both ends of decoded path text.
std::string trim_path(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), trimmed_character);
    const auto last = std::find_if_not(value.rbegin(), value.rend(), trimmed_character).base();
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
    if (trimmed.empty()) {
        return std::nullopt;
    }
    std::string resolved = normalize_path(trimmed);
    if (resolved.size() > maximum_path_size) {
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
            std::string("/sd/gcodes/.md5/") + relative,
            std::string("/sd/gcodes/.lz/") + relative,
        };
    }

    constexpr std::string_view sd_prefix = "/sd/";
    if (resolved_path.compare(0U, sd_prefix.size(), sd_prefix) == 0) {
        return {std::string("/sd/.md5/") + std::string(resolved_path.substr(sd_prefix.size())),
                std::nullopt};
    }
    return {};
}

std::optional<std::string> extract_cached_md5(BytesView content) {
    std::string result;
    const std::size_t inspected = std::min(content.size(), maximum_md5_input_size);
    for (std::size_t index = 0U;
         index < inspected && result.size() < md5_text_size;
         ++index) {
        const unsigned char character = content[index];
        if (std::isxdigit(character) != 0) {
            result.push_back(static_cast<char>(std::tolower(character)));
        }
    }
    if (result.size() != md5_text_size) {
        return std::nullopt;
    }
    return result;
}

}  // namespace firmware::core
