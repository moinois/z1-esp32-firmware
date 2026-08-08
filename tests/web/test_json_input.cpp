// Verifies bounded JSON-prefix parsing and case-insensitive object lookup.
#include "test.hpp"

#include "firmware/core/json_input.hpp"

#include <string_view>
#include <array>

using firmware::core::find_json_number;
using firmware::core::find_json_string;
using firmware::core::parse_json_prefix;

TEST_CASE(web_006_json_parser_accepts_one_value_and_ignores_trailing_bytes) {
    const auto document = parse_json_prefix(
        std::string_view("{\"Resolution\":7}trailing bytes"));
    REQUIRE(document.has_value());
    REQUIRE_EQ(find_json_number(*document, "resolution"), std::optional<double>(7.0));
}

TEST_CASE(web_006_json_parser_stops_at_embedded_nul) {
    const char input[] = "{\"resolution\":3}\0not-json";
    const auto document = parse_json_prefix(
        firmware::core::BytesView(
            reinterpret_cast<const std::uint8_t*>(input), sizeof(input) - 1U));
    REQUIRE(document.has_value());
    REQUIRE_EQ(find_json_number(*document, "resolution"), std::optional<double>(3.0));
}

TEST_CASE(web_006_json_member_lookup_uses_first_case_insensitive_duplicate) {
    const auto document = parse_json_prefix(
        std::string_view("{\"RESOLUTION\":4,\"resolution\":9,\"Command\":\"Start\"}"));
    REQUIRE(document.has_value());
    REQUIRE_EQ(find_json_number(*document, "Resolution"), std::optional<double>(4.0));
    REQUIRE_EQ(find_json_string(*document, "command"),
               std::optional<std::string>("Start"));
    REQUIRE(!find_json_string(*document, "COMMAND").has_value() ||
            find_json_string(*document, "COMMAND") == std::optional<std::string>("Start"));
}

TEST_CASE(web_006_json_parser_rejects_invalid_or_non_object_values) {
    REQUIRE(!parse_json_prefix(std::string_view("{\"resolution\":}" )).has_value());
    REQUIRE(!parse_json_prefix(std::string_view(" ")).has_value());
    REQUIRE(parse_json_prefix(std::string_view("[1,2,3] trailing")).has_value());
}

TEST_CASE(web_006_first_duplicate_member_wins_even_when_its_type_is_wrong) {
    const auto document = parse_json_prefix(
        std::string_view("{\"resolution\":\"seven\",\"RESOLUTION\":7}"));
    REQUIRE(document.has_value());
    REQUIRE(!find_json_number(*document, "resolution").has_value());
}

TEST_CASE(web_006_json_parser_accepts_scalars_composites_and_short_escapes) {
    const auto document = parse_json_prefix(std::string_view(
        "{\"truth\":true,\"lie\":false,\"none\":null,"
        "\"object\":{\"nested\":1},\"array\":[1,{\"x\":2}],"
        "\"escaped\":\"\\\"\\\\\\/\\b\\f\\n\\r\\t\","
        "\"number\":-1.25e+2}"));

    REQUIRE(document.has_value());
    REQUIRE_EQ(find_json_string(*document, "escaped"),
               std::optional<std::string>("\"\\/\b\f\n\r\t"));
    REQUIRE_EQ(find_json_number(*document, "number"),
               std::optional<double>(-125.0));
    REQUIRE(!find_json_number(*document, "truth").has_value());
    REQUIRE(!find_json_string(*document, "missing").has_value());
}

TEST_CASE(web_006_json_parser_rejects_truncated_and_malformed_tokens) {
    constexpr std::array invalid{
        std::string_view("{"),
        std::string_view("{\"name\"}"),
        std::string_view("{\"name\":"),
        std::string_view("{\"name\":?}"),
        std::string_view("{\"name\":1e}"),
        std::string_view("{\"name\":\"unterminated}"),
        std::string_view("{\"name\":\"bad\\x\"}"),
        std::string_view("{\"name\":\"bad\\"),
        std::string_view("{\"nested\":[1,2}"),
        std::string_view("{\"nested\":{\"x\":1}"),
        std::string_view("{\"a\":1 \"b\":2}"),
    };

    for (const auto input : invalid) {
        REQUIRE(!parse_json_prefix(input).has_value());
    }
    const std::string control{"{\"name\":\"bad\x01\"}"};
    REQUIRE(!parse_json_prefix(std::string_view(control)).has_value());
}
