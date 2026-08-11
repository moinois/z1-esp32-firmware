// Tests common escaping, path normalization, and first-match command recognition.
#include "test.hpp"
#include "core/protocol/text.hpp"

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

TEST_CASE(cmd_002_argument_offsets_are_empty_until_the_required_suffix_byte) {
    const ByteVector prefixes[] = {
        {'m', 'd', '5', 's', 'u', 'm'},
        {'c', 'o', 'n', 'f', 'i', 'g', '-', 'g', 'e', 't'},
        {'t', 'i', 'm', 'e'},
    };
    for (const auto& prefix : prefixes) {
        const auto match = firmware::core::recognize_command(prefix);
        REQUIRE(match.accepted);
        REQUIRE_EQ(match.argument_offset, prefix.size());
    }
    const ByteVector md5{'m', 'd', '5', 's', 'u', 'm', ' ', 'x'};
    REQUIRE_EQ(firmware::core::recognize_command(md5).argument_offset, 7U);
}

TEST_CASE(cmd_002_complete_payload_commands_keep_offset_zero) {
    const ByteVector wlan{'w', 'l', 'a', 'n', ' ', 's', 's', 'i', 'd'};
    const ByteVector serial{'s', 'n', '-', 's', 'e', 't', ' ', '1', '2'};
    REQUIRE_EQ(firmware::core::recognize_command(wlan).argument_offset, 0U);
    REQUIRE_EQ(firmware::core::recognize_command(serial).argument_offset, 0U);
}

TEST_CASE(apcmd_001_ap_requires_nul_or_ascii_whitespace_boundary) {
    using firmware::core::CommandKind;
    REQUIRE_EQ(firmware::core::recognize_command(ByteVector{'a', 'p', ' '}).kind,
               CommandKind::access_point);
    REQUIRE_EQ(firmware::core::recognize_command(ByteVector{'a', 'p', '\t'}).kind,
               CommandKind::access_point);
    REQUIRE_EQ(firmware::core::recognize_command(ByteVector{'a', 'p', 0}).kind,
               CommandKind::access_point);
    REQUIRE_EQ(firmware::core::recognize_command(ByteVector{'a', 'p', 'x'}).kind,
               CommandKind::unknown);
    REQUIRE_EQ(firmware::core::recognize_command(ByteVector{'A', 'P', ' '}).kind,
               CommandKind::unknown);
}

TEST_CASE(cmd_002_ap_literal_space_exposes_bytes_from_offset_three) {
    const auto spaced = firmware::core::recognize_command(
        ByteVector{'a', 'p', ' ', 'g', 'e', 't'});
    REQUIRE_EQ(spaced.argument_offset, 3U);

    const auto tabbed = firmware::core::recognize_command(
        ByteVector{'a', 'p', '\t', 'g', 'e', 't'});
    REQUIRE_EQ(tabbed.argument_offset, 6U);
}

TEST_CASE(apq_001_m482_and_m483_are_bounded_case_sensitive_prefixes) {
    using firmware::core::CommandKind;
    REQUIRE_EQ(firmware::core::recognize_command(
                   ByteVector{'M', '4', '8', '2', '.', '7'}).kind,
               CommandKind::station_parameter_query);
    REQUIRE_EQ(firmware::core::recognize_command(
                   ByteVector{'M', '4', '8', '3', 'x'}).kind,
               CommandKind::access_point_parameter_query);
    REQUIRE_EQ(firmware::core::recognize_command(
                   ByteVector{'m', '4', '8', '2'}).kind,
               CommandKind::unknown);

    ByteVector overlong(129U, 'x');
    std::copy_n("M482", 4U, overlong.begin());
    REQUIRE(!firmware::core::recognize_command(overlong).accepted);
}

TEST_CASE(cmd_mock_sd_control_exposes_only_its_action_as_argument) {
    const ByteVector command{'m', 'o', 'c', 'k', '-', 's', 'd', ' ',
                             'f', 'a', 'i', 'l', '-', 'r', 'e', 'a', 'd'};
    const auto match = firmware::core::recognize_command(command);
    REQUIRE_EQ(match.kind, firmware::core::CommandKind::mock_sd_control);
    REQUIRE_EQ(match.argument_offset, 7U);
    REQUIRE(match.accepted);
}

TEST_CASE(cmd_mock_nvs_control_exposes_only_its_action_as_argument) {
    const ByteVector command{'m', 'o', 'c', 'k', '-', 'n', 'v', 's', ' ',
                             'f', 'a', 'i', 'l', '-', 'o', 'p', 'e', 'n'};
    const auto match = firmware::core::recognize_command(command);
    REQUIRE_EQ(match.kind, firmware::core::CommandKind::mock_nvs_control);
    REQUIRE_EQ(match.argument_offset, 8U);
    REQUIRE(match.accepted);
}

TEST_CASE(cmd_mock_network_control_exposes_only_its_action_as_argument) {
    const ByteVector command{'m', 'o', 'c', 'k', '-', 'n', 'e', 't', ' ',
                             'f', 'a', 'i', 'l', '-', 't', 'c', 'p'};
    const auto match = firmware::core::recognize_command(command);
    REQUIRE_EQ(match.kind, firmware::core::CommandKind::mock_network_control);
    REQUIRE_EQ(match.argument_offset, 8U);
    REQUIRE(match.accepted);
}

TEST_CASE(cmd_003_only_the_named_commands_are_unbounded) {
    const char* unlimited[] = {"?", "ftype", "M951", "M952", "upgrade",
                               "reset", "diagnose", "version"};
    for (const char* prefix : unlimited) {
        ByteVector payload(200U, 'x');
        const std::size_t length = std::char_traits<char>::length(prefix);
        std::copy(prefix, prefix + length, payload.begin());
        REQUIRE(firmware::core::recognize_command(payload).accepted);
    }
    ByteVector bounded(129U, 'x');
    bounded[0] = 't';
    bounded[1] = 'i';
    bounded[2] = 'm';
    bounded[3] = 'e';
    REQUIRE(!firmware::core::recognize_command(bounded).accepted);
}
