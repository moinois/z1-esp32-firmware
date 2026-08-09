/** @file @brief Implements common path cleanup and bounded directory-list option parsing. */
#include "core/filesystem/filesystem_syntax.hpp"

#include "core/filesystem/sd_user_path.hpp"
#include "core/protocol/text.hpp"

#include <string_view>
#include <utility>

namespace firmware::core {
namespace {

constexpr std::size_t list_option_storage_size = 63U;
constexpr std::size_t required_unused_option_bytes = 2U;
constexpr std::string_view detail_option = "-s";
constexpr std::string_view ignored_suffix_marker = " -";

// Reports whether a trailing character is removed from a path argument.
bool is_trailing_path_space(char character) {
    return character == '\t' || character == '\n' || character == '\r' ||
           character == ' ';
}

// Applies common whitespace and suffix rules to already decoded text.
std::string clean_path_text(std::string text) {
    const std::size_t first = text.find_first_not_of(' ');
    if (first == std::string::npos) {
        return {};
    }
    text.erase(0U, first);

    while (!text.empty() && is_trailing_path_space(text.back())) {
        text.pop_back();
    }

    const std::size_t suffix = text.rfind(ignored_suffix_marker);
    if (suffix != std::string::npos) {
        text.erase(suffix);
    }
    return text;
}

// Skips literal spaces used as directory-list token separators.
void skip_spaces(std::string_view text, std::size_t& cursor) {
    while (cursor < text.size() && text[cursor] == ' ') {
        ++cursor;
    }
}

}  // namespace

std::optional<std::string> parse_filesystem_path(BytesView argument) {
    std::string path = clean_path_text(decode_escaped(argument));
    if (path.empty()) {
        return std::nullopt;
    }
    return resolve_sd_user_path(path);
}

std::optional<DirectoryListArguments> parse_directory_list_arguments(
    BytesView argument) {
    const std::string decoded = decode_escaped(argument);
    const std::string_view text(decoded);
    std::string combined_options;
    std::size_t cursor = 0U;
    skip_spaces(text, cursor);

    while (cursor < text.size() && text[cursor] == '-') {
        const std::size_t end = text.find(' ', cursor);
        const std::size_t token_end =
            end == std::string_view::npos ? text.size() : end;
        const std::string_view option = text.substr(cursor, token_end - cursor);
        const std::size_t retained_size = combined_options.size() + option.size();
        if (retained_size + required_unused_option_bytes <=
            list_option_storage_size) {
            combined_options.append(option);
        }
        cursor = token_end;
        skip_spaces(text, cursor);
    }

    std::string path = clean_path_text(std::string(text.substr(cursor)));
    path = resolve_sd_user_path(path.empty() ? "." : path);

    return DirectoryListArguments{
        std::move(path),
        combined_options.find(detail_option) != std::string::npos,
    };
}

std::optional<std::string> parse_remove_path(BytesView argument) {
    std::string decoded = decode_escaped(argument);
    const std::size_t first = decoded.find_first_not_of(' ');
    if (first == std::string::npos) {
        return std::nullopt;
    }
    decoded.erase(0U, first);
    const bool recursive_option =
        decoded.size() >= 3U && decoded[0] == '-' &&
        (decoded[1] == 'r' || decoded[1] == 'R') && decoded[2] == ' ';
    if (recursive_option) {
        decoded.erase(0U, 3U);
    }

    std::string path = clean_path_text(std::move(decoded));
    if (path.empty()) {
        return std::nullopt;
    }
    return resolve_sd_user_path(path);
}

std::optional<MovePaths> parse_move_paths(BytesView argument) {
    std::string decoded = decode_escaped(argument);
    const std::size_t first = decoded.find_first_not_of(' ');
    if (first == std::string::npos) {
        return std::nullopt;
    }
    decoded.erase(0U, first);

    std::size_t separator = decoded.find(" /");
    if (separator == std::string::npos) {
        separator = decoded.find(' ');
    }
    if (separator == std::string::npos) {
        return std::nullopt;
    }

    std::string source = clean_path_text(decoded.substr(0U, separator));
    std::string destination = clean_path_text(decoded.substr(separator + 1U));
    if (source.empty() || destination.empty()) {
        return std::nullopt;
    }
    return MovePaths{
        resolve_sd_user_path(source),
        resolve_sd_user_path(destination),
    };
}

}  // namespace firmware::core
