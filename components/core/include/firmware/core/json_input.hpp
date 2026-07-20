// Declares bounded JSON-prefix parsing for web request payloads.
#pragma once

#include "firmware/core/bytes.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::core {

// Stores scalar members from one parsed top-level JSON object.
struct JsonMember {
    std::string name;
    std::optional<double> number;
    std::optional<std::string> string;
};

// Owns the members extracted from one valid JSON value at the input start.
struct JsonDocument {
    std::vector<JsonMember> members;
};

// Parses one top-level object up to its closing value and ignores later bytes.
std::optional<JsonDocument> parse_json_prefix(BytesView input);

// Finds the first numeric member whose ASCII name matches case-insensitively.
std::optional<double> find_json_number(const JsonDocument& document,
                                       std::string_view member_name);

// Finds the first string member whose ASCII name matches case-insensitively.
std::optional<std::string> find_json_string(const JsonDocument& document,
                                            std::string_view member_name);

}  // namespace firmware::core
