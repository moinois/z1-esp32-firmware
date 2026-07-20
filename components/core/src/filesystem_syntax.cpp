// Implements common path cleanup and bounded directory-list option parsing.
#include "firmware/core/filesystem_syntax.hpp"

#include "firmware/core/text.hpp"

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
    return normalize_path(std::move(path));
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
    if (path.empty()) {
        path = ".";
    } else {
        path = normalize_path(std::move(path));
    }

    return DirectoryListArguments{
        std::move(path),
        combined_options.find(detail_option) != std::string::npos,
    };
}

}  // namespace firmware::core
