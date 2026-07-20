// Declares reusable parsing for configuration files and command tokens.
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::core {

// Holds one parsed configuration key and value without storage dependencies.
struct ConfigurationEntry {
    std::string key;
    std::string value;
};

// Parses one SD configuration line using delimiter and comment rules.
std::optional<ConfigurationEntry> parse_sd_config_line(std::string_view line);

// Parses one live-view chunk while retaining inline comment text.
std::optional<ConfigurationEntry> parse_live_config_chunk(BytesView chunk);

// Splits raw command bytes on literal spaces before decoding each token.
std::vector<std::string> parse_configuration_tokens(BytesView argument,
                                                    std::size_t maximum_tokens);

// Rewrites every matching SD entry or appends one missing key.
std::string rewrite_sd_configuration(std::string_view content,
                                     std::string_view key,
                                     std::string_view value);

}  // namespace firmware::core
