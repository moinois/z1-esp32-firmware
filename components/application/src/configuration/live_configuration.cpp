// Implements bounded lazy configuration loading and first-match update semantics.
#include "firmware/application/live_configuration.hpp"

#include <algorithm>
#include <string>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_chunk_size = 511U;
constexpr std::size_t maximum_entries = 100U;
constexpr std::size_t maximum_key_size = 63U;
constexpr std::size_t maximum_value_size = 255U;

// Copies at most one field's retained byte limit.
std::string bounded_text(std::string_view text, std::size_t maximum_size) {
    return std::string(text.substr(0U, maximum_size));
}

}  // namespace

void LiveConfiguration::ensure_loaded(LiveConfigurationPort& port) {
    if (loaded_) {
        return;
    }
    loaded_ = true;
    entries_.clear();
    const auto chunks = port.read_configuration_chunks(maximum_chunk_size);
    if (!chunks.has_value()) {
        return;
    }

    for (const core::ByteVector& chunk : *chunks) {
        if (entries_.size() == maximum_entries) {
            break;
        }
        const auto parsed = core::parse_live_config_chunk(chunk);
        if (!parsed.has_value()) {
            continue;
        }
        entries_.push_back({
            bounded_text(parsed->key, maximum_key_size),
            bounded_text(parsed->value, maximum_value_size),
        });
    }
}

void LiveConfiguration::reset() {
    entries_.clear();
    loaded_ = false;
}

std::optional<std::string> LiveConfiguration::find(std::string_view key) const {
    const auto found = std::find_if(
        entries_.begin(), entries_.end(), [key](const auto& entry) {
            return entry.key == key;
        });
    if (found == entries_.end()) {
        return std::nullopt;
    }
    return found->value;
}

bool LiveConfiguration::set(std::string_view key, std::string_view value) {
    const auto found = std::find_if(
        entries_.begin(), entries_.end(), [key](const auto& entry) {
            return entry.key == key;
        });
    if (found != entries_.end()) {
        found->value = bounded_text(value, maximum_value_size);
        return true;
    }
    if (entries_.size() == maximum_entries) {
        return false;
    }
    entries_.push_back({
        bounded_text(key, maximum_key_size),
        bounded_text(value, maximum_value_size),
    });
    return true;
}

std::size_t LiveConfiguration::entry_count() const {
    return entries_.size();
}

const std::vector<core::ConfigurationEntry>& LiveConfiguration::entries() const {
    return entries_;
}

}  // namespace firmware::application
