// Verifies M951/M952 recording state transitions and exact response frames.
#include "test.hpp"
#include "firmware/application/recording_commands.hpp"

TEST_CASE(rec_001_recording_commands_return_general_ok) {
    auto start = firmware::application::handle_recording_command(
        firmware::core::CommandKind::record_start, false);
    REQUIRE(start.recognized);
    REQUIRE(start.requested);
    REQUIRE_EQ(start.response.type, firmware::core::protocol::general_command);
    REQUIRE_EQ(start.response.payload, firmware::core::ByteVector({'o', 'k', '\n'}));
    const auto stop = firmware::application::handle_recording_command(
        firmware::core::CommandKind::record_stop, true);
    REQUIRE(stop.recognized);
    REQUIRE(!stop.requested);
    REQUIRE_EQ(stop.response.payload, firmware::core::ByteVector({'o', 'k', '\n'}));
}

TEST_CASE(rec_001_unknown_command_is_silent) {
    const auto result = firmware::application::handle_recording_command(
        firmware::core::CommandKind::status, false);
    REQUIRE(!result.recognized);
}
