// Implements ASCII trimming, SD line parsing, and pre-decode token splitting.
#include "firmware/core/configuration_syntax.hpp"

#include "firmware/core/text.hpp"

namespace firmware::core {
namespace {

// Reports whether a byte is one of the six ASCII whitespace characters.
bool is_ascii_whitespace(char character) {
    return character == ' ' || character == '\t' || character == '\n' ||
           character == '\r' || character == '\v' || character == '\f';
}

// Returns a view with ASCII whitespace removed from both ends.
std::string_view trim_ascii(std::string_view text) {
    while (!text.empty() && is_ascii_whitespace(text.front())) {
        text.remove_prefix(1U);
    }
    while (!text.empty() && is_ascii_whitespace(text.back())) {
        text.remove_suffix(1U);
    }
    return text;
}

}  // namespace

std::optional<ConfigurationEntry> parse_sd_config_line(std::string_view line) {
    line = trim_ascii(line);
    if (line.empty() || line.front() == ';') {
        return std::nullopt;
    }

    std::size_t delimiter = line.find('=');
    if (delimiter == std::string_view::npos) {
        delimiter = line.find(' ');
    }
    if (delimiter == std::string_view::npos) {
        return std::nullopt;
    }

    const std::string_view key = trim_ascii(line.substr(0U, delimiter));
    if (key.empty()) {
        return std::nullopt;
    }
    std::string_view value = line.substr(delimiter + 1U);
    const std::size_t comment = value.find('#');
    if (comment != std::string_view::npos) {
        value = value.substr(0U, comment);
    }
    value = trim_ascii(value);
    return ConfigurationEntry{
        std::string(key),
        std::string(value),
    };
}

std::optional<ConfigurationEntry> parse_live_config_chunk(BytesView chunk) {
    if (chunk.size() == 0U || chunk[0] == ';' || chunk[0] == '\n' ||
        chunk[0] == '\r') {
        return std::nullopt;
    }
    const std::string_view text(
        reinterpret_cast<const char*>(chunk.data()), chunk.size());
    const std::size_t delimiter = text.find('=');
    if (delimiter == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view key = trim_ascii(text.substr(0U, delimiter));
    if (key.empty()) {
        return std::nullopt;
    }
    const std::string_view value = trim_ascii(text.substr(delimiter + 1U));
    return ConfigurationEntry{
        std::string(key),
        std::string(value),
    };
}

std::vector<std::string> parse_configuration_tokens(
    BytesView argument, std::size_t maximum_tokens) {
    std::vector<std::string> tokens;
    tokens.reserve(maximum_tokens);
    std::size_t cursor = 0U;
    while (cursor < argument.size() && tokens.size() < maximum_tokens) {
        while (cursor < argument.size() && argument[cursor] == ' ') {
            ++cursor;
        }
        if (cursor == argument.size()) {
            break;
        }
        std::size_t end = cursor;
        while (end < argument.size() && argument[end] != ' ') {
            ++end;
        }
        tokens.push_back(decode_escaped(
            {argument.data() + cursor, end - cursor}));
        cursor = end;
    }
    return tokens;
}

}  // namespace firmware::core
