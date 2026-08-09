/** @file @brief Reusable parsing and rewriting of configuration text. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::core {

/** One parsed configuration assignment without storage dependencies. */
struct ConfigurationEntry {
    /// Exact key after syntax-level whitespace handling.
    std::string key;
    /// Value content with the parser-specific comment policy applied.
    std::string value;
};

/** Parses one persisted configuration line using delimiter and comment rules. */
std::optional<ConfigurationEntry> parse_sd_config_line(std::string_view line);

/** Parses a controller live-view chunk while retaining inline comments. */
std::optional<ConfigurationEntry> parse_live_config_chunk(BytesView chunk);

/** Splits on literal spaces before escape decoding each bounded token.
 *  @param argument Raw command argument bytes.
 *  @param maximum_tokens Hard bound that prevents unbounded command expansion.
 *  @return Decoded tokens in input order.
 */
std::vector<std::string> parse_configuration_tokens(BytesView argument,
                                                    std::size_t maximum_tokens);

/** Rewrites every matching entry or appends a missing key.
 *  Existing comments, unrelated lines, and line-ending behavior are preserved.
 */
std::string rewrite_sd_configuration(std::string_view content,
                                     std::string_view key,
                                     std::string_view value);

}  // namespace firmware::core
