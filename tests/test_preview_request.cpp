// Verifies preview JSON namespace, command, sequence, path, and selector rules.
#include "test.hpp"

#include "firmware/application/preview_request.hpp"

#include <string_view>

using firmware::application::PreviewCommand;
using firmware::application::parse_preview_request;

TEST_CASE(prev_001_and_002_only_valid_namespace_and_commands_are_recognized) {
    const auto request = parse_preview_request(
        "{\"ns\":\"vpreview\",\"cmd\":\"open\",\"path\":\"/sd/videos/a.avi\"}");
    REQUIRE(request.has_value());
    REQUIRE(request->command == PreviewCommand::open);
    REQUIRE_EQ(request->path, std::string("/sd/videos/a.avi"));
    REQUIRE(!parse_preview_request("{\"ns\":\"other\",\"cmd\":\"open\"}"));
    REQUIRE(!parse_preview_request("{\"ns\":\"vpreview\",\"cmd\":\"OPEN\"}"));
    REQUIRE(!parse_preview_request("not-json"));
}

TEST_CASE(prev_002_and_007_sequence_and_selectors_use_exact_defaults_and_truncation) {
    const auto request = parse_preview_request(
        "{\"ns\":\"vpreview\",\"cmd\":\"seek\",\"seq\":-2.8,"
        "\"from_ms\":-1,\"from_frame\":4.9,\"t_ms\":7.8,\"frame\":-3.2}");
    REQUIRE(request.has_value());
    REQUIRE_EQ(request->sequence, 0U);
    REQUIRE_EQ(request->from_milliseconds, 0U);
    REQUIRE_EQ(request->from_frame, 4);
    REQUIRE_EQ(request->time_milliseconds, 7U);
    REQUIRE_EQ(request->frame, -3);

    const auto defaults = parse_preview_request(
        "{\"ns\":\"vpreview\",\"cmd\":\"pause\",\"seq\":\"bad\"}");
    REQUIRE(defaults.has_value());
    REQUIRE_EQ(defaults->sequence, 0U);
    REQUIRE_EQ(defaults->from_frame, -1);
    REQUIRE_EQ(defaults->frame, -1);
}

TEST_CASE(prev_002_paths_and_sessions_are_bounded_without_changing_command_values) {
    const std::string long_path(300U, 'p');
    const std::string long_session(80U, 's');
    const std::string input = "{\"ns\":\"vpreview\",\"cmd\":\"resume\",\"path\":\"" +
                              long_path + "\",\"session_id\":\"" + long_session + "\"}";
    const auto request = parse_preview_request(input);
    REQUIRE(request.has_value());
    REQUIRE_EQ(request->path.size(), 255U);
    REQUIRE_EQ(request->session_id.size(), 39U);
    REQUIRE(request->command == PreviewCommand::resume);
}
