// Declares the target-neutral parsed representation of config.txt.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::application {

// Represents one normalized configuration key/value pair.
struct ConfigurationEntry {
    std::string key;
    std::string value;
};

// Preserves configuration text while providing structured key/value access.
class ConfigurationDocument {
public:
    // Parses comments, key/value lines, and unknown text from a complete file.
    static ConfigurationDocument parse(std::string_view text);

    // Returns the first value for an exact key.
    std::optional<std::string_view> get(std::string_view key) const;

    // Replaces the first key or appends a new key/value line.
    void set(std::string_view key, std::string_view value);

    // Returns all parsed entries in source order.
    std::vector<ConfigurationEntry> entries() const;

    // Serializes the document while retaining comments and unknown lines.
    std::string serialize() const;

    // Returns the source lines without parsing them into settings.
    std::vector<std::string> lines() const;

private:
    struct Line {
        std::string original;
        std::string key;
        std::string value;
        bool is_entry = false;
    };
    std::vector<Line> lines_;
};

// Provides access to entries below a normalized tag_ prefix.
class ConfigurationNamespace {
public:
    // Creates a namespace view over one parsed document.
    ConfigurationNamespace(ConfigurationDocument& document, std::string_view tag);

    // Reads one suffix below the namespace prefix.
    std::optional<std::string_view> get(std::string_view key) const;

    // Replaces or appends one namespaced setting.
    void set(std::string_view key, std::string_view value);

    // Returns all settings below the namespace prefix.
    std::vector<ConfigurationEntry> get_all() const;

private:
    ConfigurationDocument& document_;
    std::string prefix_;
};

}  // namespace firmware::application
