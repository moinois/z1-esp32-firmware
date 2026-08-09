/** @file @brief Bounded JSON-prefix parsing for web request payloads. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::core {

/** Scalar member from one parsed top-level JSON object. */
struct JsonMember {
    /// Member name exactly as encoded after JSON unescaping.
    std::string name;
    /// Numeric value when the member contains a supported JSON number.
    std::optional<double> number;
    /// String value when the member contains a supported JSON string.
    std::optional<std::string> string;
};

/** Owns supported members extracted from one valid top-level object. */
struct JsonDocument {
    std::vector<JsonMember> members;
};

/** Parses one top-level object and deliberately ignores later transport bytes. */
std::optional<JsonDocument> parse_json_prefix(BytesView input);

/** Finds the first numeric member with an ASCII case-insensitive name match. */
std::optional<double> find_json_number(const JsonDocument& document,
                                       std::string_view member_name);

/** Finds the first string member with an ASCII case-insensitive name match. */
std::optional<std::string> find_json_string(const JsonDocument& document,
                                            std::string_view member_name);

}  // namespace firmware::core
