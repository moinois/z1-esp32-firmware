// Tests the shared parsing rules for path-oriented filesystem commands.
#include "test.hpp"

#include "firmware/core/filesystem_syntax.hpp"

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
    REQUIRE_EQ(parsed->path, std::string("."));
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
    REQUIRE_EQ(parsed->path, std::string("."));
    REQUIRE(!parsed->include_details);
}
