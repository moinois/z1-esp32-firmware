// Implements fresh SD reads and atomic config.txt updates.
#include "configuration_file_store.hpp"

#include "firmware/application/configuration_tags.hpp"

#include <cstdio>
#include <utility>

namespace firmware::target {
namespace {

constexpr char active_path[] = "/sd/config.txt";
constexpr char temporary_path[] = "/sd/config.tmp";
constexpr std::size_t buffer_size = 256U;

std::string_view effective_tag(std::string_view tag) {
    return tag.empty() ? firmware::application::mainboard_configuration_tag : tag;
}

std::optional<std::string> read_file(std::string_view path) {
    std::FILE* file = std::fopen(std::string(path).c_str(), "rb");
    if (file == nullptr) return std::nullopt;
    std::string content;
    char buffer[buffer_size];
    while (const std::size_t count = std::fread(buffer, 1U, sizeof(buffer), file)) {
        content.append(buffer, count);
    }
    const bool failed = std::ferror(file) != 0;
    std::fclose(file);
    return failed ? std::nullopt : std::optional<std::string>(std::move(content));
}

bool write_file(std::string_view path, std::string_view content) {
    std::FILE* file = std::fopen(std::string(path).c_str(), "wb");
    if (file == nullptr) return false;
    const bool written = std::fwrite(content.data(), 1U, content.size(), file) ==
                         content.size();
    const bool flushed = std::fflush(file) == 0;
    const bool closed = std::fclose(file) == 0;
    return written && flushed && closed;
}

}  // namespace

bool ConfigurationFileStore::exists() const {
    std::FILE* file = std::fopen(active_path, "rb");
    if (file == nullptr) return false;
    std::fclose(file);
    return true;
}

firmware::application::ConfigurationDocument
ConfigurationFileStore::read_document() const {
    const auto content = read_file(active_path);
    if (!content.has_value()) {
        return firmware::application::ConfigurationDocument::parse("");
    }
    return firmware::application::ConfigurationDocument::parse(*content);
}

std::optional<std::string> ConfigurationFileStore::get(
    std::string_view tag, std::string_view key) const {
    auto document = read_document();
    firmware::application::ConfigurationNamespace configuration(
        document, effective_tag(tag));
    const auto value = configuration.get(key);
    if (!value.has_value()) return std::nullopt;
    return std::string(*value);
}

std::vector<firmware::application::ConfigurationEntry>
ConfigurationFileStore::get_all(std::string_view tag) const {
    auto document = read_document();
    firmware::application::ConfigurationNamespace configuration(
        document, effective_tag(tag));
    return configuration.get_all();
}

bool ConfigurationFileStore::set(std::string_view tag, std::string_view key,
                                 std::string_view value) const {
    auto document = read_document();
    firmware::application::ConfigurationNamespace configuration(
        document, effective_tag(tag));
    configuration.set(key, value);
    document.uppercase_keys();
    if (!write_file(temporary_path, document.serialize())) return false;
    if (std::rename(temporary_path, active_path) != 0) {
        std::remove(temporary_path);
        return false;
    }
    return true;
}

std::vector<std::string> ConfigurationFileStore::read_lines() const {
    return read_document().lines();
}

std::string_view active_configuration_path() {
    return active_path;
}

}  // namespace firmware::target
