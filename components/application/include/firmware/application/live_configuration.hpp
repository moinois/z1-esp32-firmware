// Declares the bounded retained live configuration view and its loading port.
#pragma once

#include "firmware/core/bytes.hpp"
#include "firmware/core/configuration_syntax.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::application {

// Isolates live-view loading from target filesystem chunk APIs.
class LiveConfigurationPort {
public:
    // Enables safe destruction through a substituted port implementation.
    virtual ~LiveConfigurationPort() = default;

    // Reads bounded source chunks or reports that the active file is absent.
    virtual std::optional<std::vector<core::ByteVector>> read_chunks(
        std::string_view path, std::size_t maximum_chunk_size) = 0;
};

// Retains a lazy, bounded configuration snapshot independent of SD rewrites.
class LiveConfiguration {
public:
    // Loads at most once until an operation explicitly resets the view.
    void ensure_loaded(LiveConfigurationPort& port);

    // Discards all retained entries and permits a later fresh load.
    void reset();

    // Returns the first exact-key value when present.
    std::optional<std::string> find(std::string_view key) const;

    // Updates the first exact key or appends one bounded entry when space remains.
    bool set(std::string_view key, std::string_view value);

    // Reports the number of retained entries including duplicates.
    std::size_t entry_count() const;

    // Exposes retained entries read-only for deterministic integration.
    const std::vector<core::ConfigurationEntry>& entries() const;

private:
    std::vector<core::ConfigurationEntry> entries_;
    bool loaded_ = false;
};

}  // namespace firmware::application
