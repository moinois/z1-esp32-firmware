// Verifies local command families remain independent from transport code.
#include "test.hpp"
#include "firmware/application/local_command_classifier.hpp"

TEST_CASE(local_classifier_maps_runtime_commands) {
    REQUIRE_EQ(firmware::application::classify_local_command(
                    firmware::core::CommandKind::system_time),
               firmware::application::LocalCommandFamily::runtime);
    REQUIRE_EQ(firmware::application::classify_local_command(
                    firmware::core::CommandKind::clear_first_time),
               firmware::application::LocalCommandFamily::runtime);
}

TEST_CASE(local_classifier_maps_recording_and_unknown_commands) {
    REQUIRE_EQ(firmware::application::classify_local_command(
                    firmware::core::CommandKind::record_start),
               firmware::application::LocalCommandFamily::recording);
    REQUIRE_EQ(firmware::application::classify_local_command(
                    firmware::core::CommandKind::unknown),
               firmware::application::LocalCommandFamily::none);
}
