/** @file @brief Implements parsing, namespaced lookup, and lossless config serialization. */
#include "application/configuration/configuration_document.hpp"

#include <algorithm>

namespace firmware::application {
namespace {

bool space(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
           value == '\f' || value == '\v';
}

char lower_ascii(char value) {
    return value >= 'A' && value <= 'Z'
               ? static_cast<char>(value - 'A' + 'a')
               : value;
}

std::string uppercase_ascii(std::string_view value) {
    std::string result(value);
    for (char& character : result) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return result;
}

bool equal_case_insensitive(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0U; index < left.size(); ++index) {
        if (lower_ascii(left[index]) != lower_ascii(right[index])) return false;
    }
    return true;
}

bool begins_case_insensitive(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() &&
           equal_case_insensitive(value.substr(0U, prefix.size()), prefix);
}

std::string_view trim(std::string_view value) {
    while (!value.empty() && space(value.front())) value.remove_prefix(1U);
    while (!value.empty() && space(value.back())) value.remove_suffix(1U);
    return value;
}

}  // namespace

ConfigurationDocument ConfigurationDocument::parse(std::string_view text) {
    ConfigurationDocument document;
    std::size_t start = 0U;
    while (start < text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::size_t limit = end == std::string_view::npos ? text.size() : end;
        std::string original(text.substr(start, limit - start));
        if (!original.empty() && original.back() == '\r') original.pop_back();
        const std::string_view content = trim(original);
        Line line;
        line.original = original;
        if (!content.empty() && content.front() != '#' && content.front() != ';') {
            std::size_t separator = content.find('=');
            if (separator == std::string_view::npos) {
                separator = content.find_first_of(" \t");
            }
            if (separator != std::string_view::npos) {
                const std::string_view key = trim(content.substr(0U, separator));
                const std::string_view value = trim(content.substr(separator + 1U));
                if (!key.empty()) {
                    line.key = key;
                    line.value = value;
                    line.is_entry = true;
                }
            }
        }
        document.lines_.push_back(std::move(line));
        if (end == std::string_view::npos) break;
        start = end + 1U;
    }
    return document;
}

std::optional<std::string_view> ConfigurationDocument::get(
    std::string_view key) const {
    for (const Line& line : lines_) {
        if (line.is_entry && equal_case_insensitive(line.key, key)) {
            return line.value;
        }
    }
    return std::nullopt;
}

void ConfigurationDocument::set(std::string_view key, std::string_view value) {
    const std::string canonical_key = uppercase_ascii(key);
    for (Line& line : lines_) {
        if (line.is_entry && equal_case_insensitive(line.key, key)) {
            line.original = canonical_key + "=" + std::string(value);
            line.key = canonical_key;
            line.value = value;
            return;
        }
    }
    lines_.push_back({canonical_key + "=" + std::string(value), canonical_key,
                      std::string(value), true});
}

void ConfigurationDocument::uppercase_keys() {
    for (Line& line : lines_) {
        if (!line.is_entry) continue;
        line.key = uppercase_ascii(line.key);
        line.original = line.key + "=" + line.value;
    }
}

std::vector<ConfigurationEntry> ConfigurationDocument::entries() const {
    std::vector<ConfigurationEntry> result;
    for (const Line& line : lines_) {
        if (line.is_entry) result.push_back({line.key, line.value});
    }
    return result;
}

std::string ConfigurationDocument::serialize() const {
    std::string result;
    for (std::size_t index = 0U; index < lines_.size(); ++index) {
        if (index != 0U) result.push_back('\n');
        result += lines_[index].original;
    }
    if (!lines_.empty()) result.push_back('\n');
    return result;
}

std::vector<std::string> ConfigurationDocument::lines() const {
    std::vector<std::string> result;
    for (const Line& line : lines_) result.push_back(line.original);
    return result;
}

ConfigurationNamespace::ConfigurationNamespace(ConfigurationDocument& document,
                                               std::string_view tag)
    : document_(document), prefix_(tag) {
    if (!prefix_.empty() && prefix_.back() != '_') prefix_.push_back('_');
}

std::optional<std::string_view> ConfigurationNamespace::get(
    std::string_view key) const {
    return document_.get(prefix_ + std::string(key));
}

void ConfigurationNamespace::set(std::string_view key, std::string_view value) {
    document_.set(prefix_ + std::string(key), value);
}

std::vector<ConfigurationEntry> ConfigurationNamespace::get_all() const {
    std::vector<ConfigurationEntry> result;
    for (ConfigurationEntry entry : document_.entries()) {
        if (begins_case_insensitive(entry.key, prefix_)) {
            entry.key.erase(0U, prefix_.size());
            result.push_back(std::move(entry));
        }
    }
    return result;
}

}  // namespace firmware::application
