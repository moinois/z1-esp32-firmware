// Tests common escaping, path normalization, and first-match command recognition.
#include "test.hpp"
#include "firmware/core/text.hpp"

using firmware::core::ByteVector;

TEST_CASE(esc_001_escape_replacement_is_one_for_one_and_not_recursive) {
    const ByteVector input{'a', 0x01, 0x02, 0x03, 0x04, 0x05};
    REQUIRE_EQ(firmware::core::decode_escaped(input), std::string("a ?*!~"));
}

TEST_CASE(esc_002_first_nul_terminates_text) {
    REQUIRE_EQ(firmware::core::decode_escaped(ByteVector{'a', 0, 'b'}), std::string("a"));
}

TEST_CASE(hft_004_path_normalization_handles_both_separators_and_parent_components) {
    REQUIRE_EQ(firmware::core::normalize_path("foo\\bar/../baz"), std::string("/foo/baz"));
    REQUIRE_EQ(firmware::core::normalize_path("../../sd/file"), std::string("/sd/file"));
}

TEST_CASE(cmd_001_recognition_is_case_sensitive_prefix_matching) {
    REQUIRE_EQ(firmware::core::recognize_command(ByteVector{'l', 's', 'x'}).kind, firmware::core::CommandKind::list);
    REQUIRE_EQ(firmware::core::recognize_command(ByteVector{'L', 'S'}).kind, firmware::core::CommandKind::unknown);
    REQUIRE_EQ(firmware::core::recognize_command(ByteVector{'?', 'x'}).kind, firmware::core::CommandKind::status);
}

TEST_CASE(cmd_003_selected_commands_bypass_the_128_byte_limit) {
    ByteVector version(200, 'x');
    const char prefix[] = "version";
    std::copy(prefix, prefix + 7, version.begin());
    REQUIRE(firmware::core::recognize_command(version).accepted);

    ByteVector list(200, 'x');
    list[0] = 'l';
    list[1] = 's';
    REQUIRE(!firmware::core::recognize_command(list).accepted);
}
