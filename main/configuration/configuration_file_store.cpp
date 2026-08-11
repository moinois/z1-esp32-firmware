/** @file @brief Implements fresh SD reads and atomic config.txt updates. */
#include "configuration_file_store.hpp"

#include "application/configuration/configuration_tags.hpp"
#include "core/configuration/configuration_syntax.hpp"
#include "core/filesystem/sd_user_path.hpp"

#include <cstdio>
#include <mutex>
#include <utility>

namespace firmware::target {
namespace {

const std::string active_path = firmware::core::physical_sd_path("/config.txt");
const std::string temporary_path = firmware::core::physical_sd_path("/config.tmp");
constexpr std::size_t buffer_size = 256U;
std::mutex configuration_file_mutex;

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
    std::lock_guard<std::mutex> lock(configuration_file_mutex);
    std::FILE* file = std::fopen(active_path.c_str(), "rb");
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
    std::lock_guard<std::mutex> lock(configuration_file_mutex);
    auto document = read_document();
    firmware::application::ConfigurationNamespace configuration(
        document, effective_tag(tag));
    const auto value = configuration.get(key);
    if (!value.has_value()) return std::nullopt;
    return std::string(*value);
}

std::optional<std::string> ConfigurationFileStore::get_hashed(
    std::string_view tag, std::string_view key) const {
    std::lock_guard<std::mutex> lock(configuration_file_mutex);
    auto document = read_document();
    std::string prefix(effective_tag(tag));
    if (!prefix.empty() && prefix.back() != '_') prefix.push_back('_');
    const std::uint16_t requested_hash = firmware::core::configuration_hash(key);
    for (const auto& entry : document.entries()) {
        if (entry.key.size() < prefix.size()) continue;
        bool matching_prefix = true;
        for (std::size_t index = 0U; index < prefix.size(); ++index) {
            const auto lower = [](char value) {
                return value >= 'A' && value <= 'Z'
                           ? static_cast<char>(value - 'A' + 'a')
                           : value;
            };
            if (lower(prefix[index]) != lower(entry.key[index])) {
                matching_prefix = false;
                break;
            }
        }
        if (matching_prefix &&
            firmware::core::configuration_hash(
                std::string_view(entry.key).substr(prefix.size())) == requested_hash) {
            return entry.value;
        }
    }
    return std::nullopt;
}

std::vector<firmware::application::ConfigurationEntry>
ConfigurationFileStore::get_all(std::string_view tag) const {
    std::lock_guard<std::mutex> lock(configuration_file_mutex);
    auto document = read_document();
    firmware::application::ConfigurationNamespace configuration(
        document, effective_tag(tag));
    return configuration.get_all();
}

bool ConfigurationFileStore::set(std::string_view tag, std::string_view key,
                                 std::string_view value) const {
    // USB and TCP dispatch on independent tasks. Serialize the complete
    // read-modify-replace transaction so their shared config.tmp cannot be
    // renamed by one request while the other is still writing it, and so a
    // concurrent update cannot silently discard an already accepted value.
    std::lock_guard<std::mutex> lock(configuration_file_mutex);
    auto document = read_document();
    firmware::application::ConfigurationNamespace configuration(
        document, effective_tag(tag));
    configuration.set(key, value);
    document.uppercase_keys();
    if (!write_file(temporary_path, document.serialize())) return false;
    // CFG-031 requires an unlink attempt before rename. FAT does not replace
    // an existing destination atomically, so omitting this best-effort step
    // makes every update of an existing config.txt fail on the target.
    static_cast<void>(std::remove(active_path.c_str()));
    if (std::rename(temporary_path.c_str(), active_path.c_str()) != 0) {
        std::remove(temporary_path.c_str());
        return false;
    }
    return true;
}

bool ConfigurationFileStore::set_raw_key(std::string_view key,
                                         std::string_view value) const {
    std::lock_guard<std::mutex> lock(configuration_file_mutex);
    auto document = read_document();
    document.set(key, value);
    document.uppercase_keys();
    if (!write_file(temporary_path, document.serialize())) return false;
    static_cast<void>(std::remove(active_path.c_str()));
    if (std::rename(temporary_path.c_str(), active_path.c_str()) != 0) {
        std::remove(temporary_path.c_str());
        return false;
    }
    return true;
}

std::vector<std::string> ConfigurationFileStore::read_lines() const {
    std::lock_guard<std::mutex> lock(configuration_file_mutex);
    return read_document().lines();
}

std::string_view active_configuration_path() {
    return active_path;
}

}  // namespace firmware::target
