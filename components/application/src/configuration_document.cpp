// Implements parsing, namespaced lookup, and lossless config serialization.
#include "firmware/application/configuration_document.hpp"

#include <algorithm>

namespace firmware::application {
namespace {

bool space(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
           value == '\f' || value == '\v';
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
        if (line.is_entry && line.key == key) return line.value;
    }
    return std::nullopt;
}

void ConfigurationDocument::set(std::string_view key, std::string_view value) {
    for (Line& line : lines_) {
        if (line.is_entry && line.key == key) {
            line.original = std::string(key) + "=" + std::string(value);
            line.value = value;
            return;
        }
    }
    lines_.push_back({std::string(key) + "=" + std::string(value),
                      std::string(key), std::string(value), true});
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
        if (entry.key.rfind(prefix_, 0U) == 0U) {
            entry.key.erase(0U, prefix_.size());
            result.push_back(std::move(entry));
        }
    }
    return result;
}

}  // namespace firmware::application
