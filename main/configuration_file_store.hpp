// Declares the target-owned store for the active SD configuration document.
#pragma once

#include "firmware/application/configuration_document.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::target {

// Owns config.txt paths and performs one fresh operation per request.
class ConfigurationFileStore {
public:
    // Reports whether the active configuration file exists as a readable file.
    bool exists() const;

    // Returns one value under the normalized tag_ namespace.
    std::optional<std::string> get(std::string_view tag,
                                   std::string_view key) const;

    // Returns all values under the normalized tag_ namespace.
    std::vector<firmware::application::ConfigurationEntry> get_all(
        std::string_view tag) const;

    // Updates one value through an atomic temporary-file replacement.
    bool set(std::string_view tag, std::string_view key,
             std::string_view value) const;

    // Returns complete source lines for protocols that require raw records.
    std::vector<std::string> read_lines() const;

private:
    // Reads and parses a fresh document; missing files become empty documents.
    firmware::application::ConfigurationDocument read_document() const;
};

// Exposes the canonical path to target integration code without duplicating it.
std::string_view active_configuration_path();

}  // namespace firmware::target
