// Verifies SD configuration lines and literal-space command tokenization.
#include "test.hpp"

#include "core/configuration/configuration_syntax.hpp"

#include <string>
#include <vector>

using firmware::core::ByteVector;

TEST_CASE(cfg_002_sd_lines_trim_skip_comments_and_require_a_nonempty_key) {
    REQUIRE(!firmware::core::parse_sd_config_line(" \t\r\n\v\f").has_value());
    REQUIRE(!firmware::core::parse_sd_config_line("  ; ignored = value ").has_value());
    REQUIRE(!firmware::core::parse_sd_config_line(" = value ").has_value());

    const auto parsed = firmware::core::parse_sd_config_line(" \t key \t = value \r\n");
    REQUIRE(parsed.has_value());
    REQUIRE_EQ(parsed->key, std::string("key"));
    REQUIRE_EQ(parsed->value, std::string("value"));
}

TEST_CASE(cfg_002_and_003_sd_lines_prefer_equals_or_use_first_space_and_strip_comment) {
    const auto equals = firmware::core::parse_sd_config_line(
        " key with space = first # ignored = later ");
    REQUIRE(equals.has_value());
    REQUIRE_EQ(equals->key, std::string("key with space"));
    REQUIRE_EQ(equals->value, std::string("first"));

    const auto space = firmware::core::parse_sd_config_line(
        " key    value with spaces # ignored ");
    REQUIRE(space.has_value());
    REQUIRE_EQ(space->key, std::string("key"));
    REQUIRE_EQ(space->value, std::string("value with spaces"));
}

TEST_CASE(cfg_010_tokens_split_on_literal_spaces_before_escape_decoding) {
    const ByteVector argument{' ', 's', 'd', ' ', 'm', 'y', 0x01U, 'k', 'e', 'y',
                              ' ', 'i', 'g', 'n', 'o', 'r', 'e', 'd'};
    const auto tokens = firmware::core::parse_configuration_tokens(argument, 2U);

    REQUIRE_EQ(tokens.size(), 2U);
    REQUIRE_EQ(tokens[0], std::string("sd"));
    REQUIRE_EQ(tokens[1], std::string("my key"));
}

TEST_CASE(cfg_010_tabs_and_control_whitespace_remain_inside_tokens) {
    const ByteVector argument{' ', 'l', 'i', 'v', 'e', '\t', 'k', 'e', 'y',
                              '\r', '\n', '\v', '\f'};
    const auto tokens = firmware::core::parse_configuration_tokens(argument, 3U);

    REQUIRE_EQ(tokens.size(), 1U);
    REQUIRE_EQ(tokens[0], std::string("live\tkey\r\n\v\f"));
}

TEST_CASE(cfg_010_each_token_stops_at_its_first_nul_and_extra_tokens_are_ignored) {
    const ByteVector argument{' ', 's', 'd', 0U, 'x', ' ', 'k', 'e', 'y',
                              ' ', 'v', 'a', 'l', 'u', 'e', ' ', 'e', 'x', 't', 'r', 'a'};
    const auto tokens = firmware::core::parse_configuration_tokens(argument, 3U);

    REQUIRE_EQ(tokens,
               std::vector<std::string>({"sd", "key", "value"}));
}
