// Tests the shared parsing rules for path-oriented filesystem commands.
#include "test.hpp"

#include "core/filesystem/filesystem_syntax.hpp"

using firmware::core::ByteVector;

TEST_CASE(file_001_path_arguments_decode_terminate_and_trim_protocol_whitespace) {
    const ByteVector argument{' ', ' ', 'd', 'i', 'r', 0x01U, 'n', 'a', 'm', 'e',
                              '\t', '\r', '\n', ' ', 0U, 'x'};

    const auto parsed = firmware::core::parse_filesystem_path(argument);

    REQUIRE(parsed.has_value());
    REQUIRE_EQ(*parsed, std::string("/dir name"));
}

TEST_CASE(file_002_path_arguments_remove_the_last_space_hyphen_suffix) {
    const ByteVector argument{' ', 'f', 'o', 'o', ' ', '-', 'x', ' ', 'b', 'a', 'r',
                              ' ', '-', 's', 'u', 'f', 'f', 'i', 'x'};

    const auto parsed = firmware::core::parse_filesystem_path(argument);

    REQUIRE(parsed.has_value());
    REQUIRE_EQ(*parsed, std::string("/foo -x bar"));
}

TEST_CASE(file_003_empty_filesystem_paths_are_rejected) {
    REQUIRE(!firmware::core::parse_filesystem_path(ByteVector{' ', '\t', '\r', '\n'})
                 .has_value());
}

TEST_CASE(file_010_list_consumes_options_and_defaults_an_empty_path_to_dot) {
    const auto parsed = firmware::core::parse_directory_list_arguments(
        ByteVector{' ', '-', 'a', ' ', '-', 's', ' '});

    REQUIRE(parsed.has_value());
    REQUIRE_EQ(parsed->path, std::string("/"));
    REQUIRE(parsed->include_details);
}

TEST_CASE(file_010_list_decodes_the_remaining_path_after_consuming_options) {
    const auto parsed = firmware::core::parse_directory_list_arguments(
        ByteVector{' ', '-', 'a', ' ', 'g', 'c', 'o', 'd', 'e', 's', 0x01U,
                   '2', '0', '2', '6', ' ', '-', 'i', 'g', 'n', 'o', 'r', 'e'});

    REQUIRE(parsed.has_value());
    REQUIRE_EQ(parsed->path, std::string("/gcodes 2026"));
    REQUIRE(!parsed->include_details);
}

TEST_CASE(file_010_omits_an_option_that_would_leave_less_than_two_free_bytes) {
    ByteVector argument{' ', '-'};
    argument.insert(argument.end(), 60U, 'a');
    argument.insert(argument.end(), {' ', '-', 's', ' '});

    const auto parsed = firmware::core::parse_directory_list_arguments(argument);

    REQUIRE(parsed.has_value());
    REQUIRE_EQ(parsed->path, std::string("/"));
    REQUIRE(!parsed->include_details);
}

TEST_CASE(file_022_remove_accepts_either_recursive_option_spelling) {
    const auto lower = firmware::core::parse_remove_path(
        ByteVector{' ', '-', 'r', ' ', '/', 's', 'd', '/', 'o', 'n', 'e'});
    const auto upper = firmware::core::parse_remove_path(
        ByteVector{' ', '-', 'R', ' ', '/', 's', 'd', '/', 't', 'w', 'o'});

    REQUIRE(lower.has_value());
    REQUIRE(upper.has_value());
    REQUIRE_EQ(*lower, std::string("/sd/one"));
    REQUIRE_EQ(*upper, std::string("/sd/two"));
}

TEST_CASE(file_024_move_uses_space_absolute_separator_for_an_encoded_source_space) {
    const auto parsed = firmware::core::parse_move_paths(
        ByteVector{' ', 'o', 'l', 'd', 0x01U, 'n', 'a', 'm', 'e', ' ',
                   '/', 's', 'd', '/', 'n', 'e', 'w'});

    REQUIRE(parsed.has_value());
    REQUIRE_EQ(parsed->source, std::string("/old name"));
    REQUIRE_EQ(parsed->destination, std::string("/sd/new"));
}

TEST_CASE(file_024_move_falls_back_to_the_first_space_and_rejects_missing_paths) {
    const auto parsed = firmware::core::parse_move_paths(
        ByteVector{' ', 'o', 'l', 'd', ' ', 'n', 'e', 'w'});

    REQUIRE(parsed.has_value());
    REQUIRE_EQ(parsed->source, std::string("/old"));
    REQUIRE_EQ(parsed->destination, std::string("/new"));
    REQUIRE(!firmware::core::parse_move_paths(ByteVector{' ', 'o', 'n', 'l', 'y'})
                 .has_value());
}
