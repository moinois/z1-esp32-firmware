// Verifies local command families remain independent from transport code.
#include "test.hpp"
#include "application/runtime/local_command_classifier.hpp"

#include <array>

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

TEST_CASE(local_classifier_maps_every_transport_local_family) {
    using firmware::application::LocalCommandFamily;
    using firmware::core::CommandKind;
    struct ExpectedFamily {
        CommandKind command;
        LocalCommandFamily family;
    };
    constexpr std::array cases{
        ExpectedFamily{CommandKind::serial_get, LocalCommandFamily::serial_number},
        ExpectedFamily{CommandKind::serial_set, LocalCommandFamily::serial_number},
        ExpectedFamily{CommandKind::record_stop, LocalCommandFamily::recording},
        ExpectedFamily{CommandKind::list, LocalCommandFamily::filesystem},
        ExpectedFamily{CommandKind::file_type, LocalCommandFamily::filesystem},
        ExpectedFamily{CommandKind::md5_sum, LocalCommandFamily::filesystem},
        ExpectedFamily{CommandKind::remove, LocalCommandFamily::filesystem},
        ExpectedFamily{CommandKind::move, LocalCommandFamily::filesystem},
        ExpectedFamily{CommandKind::wlan, LocalCommandFamily::wlan},
        ExpectedFamily{CommandKind::status, LocalCommandFamily::none},
    };

    for (const auto& expected : cases) {
        REQUIRE_EQ(firmware::application::classify_local_command(expected.command),
                   expected.family);
    }
}
