// Verifies update-file deletion retries, permission recovery, and reporting.
#include "test.hpp"

#include "firmware/application/update_deletion.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::UpdateDeleteResult;
using firmware::application::UpdateDeletionPort;
using firmware::application::UpdateDeletionService;

namespace {

// Records filesystem recovery, delays, and host broadcasts for deletion tests.
class FakeUpdateDeletionPort final : public UpdateDeletionPort {
public:
    // Returns the next configured unlink result and records its path.
    UpdateDeleteResult unlink_file(std::string_view path) override {
        unlink_paths.emplace_back(path);
        if (unlink_results.empty()) {
            return UpdateDeleteResult::other_failure;
        }
        const UpdateDeleteResult result = unlink_results.front();
        unlink_results.erase(unlink_results.begin());
        return result;
    }

    // Attempts FAT attribute clearing and records its path.
    bool clear_fat_attributes(std::string_view path) override {
        attribute_paths.emplace_back(path);
        return attribute_clear_succeeds;
    }

    // Attempts a mode change and records both path and mode.
    bool set_mode(std::string_view path, std::uint32_t mode) override {
        mode_paths.emplace_back(path);
        modes.push_back(mode);
        return mode_change_succeeds;
    }

    // Records one exact retry delay.
    void delay_milliseconds(std::uint32_t duration) override {
        delays.push_back(duration);
    }

    // Records one host broadcast packet.
    void broadcast(std::uint8_t type, std::string_view payload) override {
        broadcast_types.push_back(type);
        broadcasts.emplace_back(payload);
    }

    bool attribute_clear_succeeds = false;
    bool mode_change_succeeds = false;
    std::vector<UpdateDeleteResult> unlink_results;
    std::vector<std::string> unlink_paths;
    std::vector<std::string> attribute_paths;
    std::vector<std::string> mode_paths;
    std::vector<std::uint32_t> modes;
    std::vector<std::uint32_t> delays;
    std::vector<std::uint8_t> broadcast_types;
    std::vector<std::string> broadcasts;
};

}  // namespace

TEST_CASE(upd_060_successful_delete_stops_after_one_attempt) {
    FakeUpdateDeletionPort port;
    port.unlink_results = {UpdateDeleteResult::success};
    UpdateDeletionService deletion(port);

    REQUIRE(deletion.remove("/sd/firmware.bin"));

    REQUIRE_EQ(port.unlink_paths.size(), 1U);
    REQUIRE(port.delays.empty());
    REQUIRE(port.broadcasts.empty());
}

TEST_CASE(upd_060_to_062_busy_delete_uses_three_attempts_and_exact_delays) {
    FakeUpdateDeletionPort port;
    port.unlink_results = {UpdateDeleteResult::busy,
                           UpdateDeleteResult::busy,
                           UpdateDeleteResult::success};
    UpdateDeletionService deletion(port);

    REQUIRE(deletion.remove("/sd/firmware.bin"));

    REQUIRE_EQ(port.unlink_paths.size(), 3U);
    REQUIRE_EQ(port.delays,
               std::vector<std::uint32_t>({500U, 200U, 500U, 200U}));
}

TEST_CASE(upd_061_permission_recovery_prefers_fat_attributes_then_retries) {
    FakeUpdateDeletionPort port;
    port.unlink_results = {UpdateDeleteResult::permission_denied,
                           UpdateDeleteResult::success};
    port.attribute_clear_succeeds = true;
    UpdateDeletionService deletion(port);

    REQUIRE(deletion.remove("/sd/lpc1768.bin"));

    REQUIRE_EQ(port.attribute_paths,
               std::vector<std::string>({"/sd/lpc1768.bin"}));
    REQUIRE(port.mode_paths.empty());
    REQUIRE_EQ(port.delays, std::vector<std::uint32_t>({100U, 200U}));
}

TEST_CASE(upd_061_permission_recovery_falls_back_to_mode_0666) {
    FakeUpdateDeletionPort port;
    port.unlink_results = {UpdateDeleteResult::permission_denied,
                           UpdateDeleteResult::success};
    port.mode_change_succeeds = true;
    UpdateDeletionService deletion(port);

    REQUIRE(deletion.remove("/sd/firmware.bin"));

    REQUIRE_EQ(port.mode_paths,
               std::vector<std::string>({"/sd/firmware.bin"}));
    REQUIRE_EQ(port.modes, std::vector<std::uint32_t>({0666U}));
    REQUIRE_EQ(port.delays, std::vector<std::uint32_t>({100U, 200U}));
}

TEST_CASE(upd_061_failed_permission_adjustments_stop_immediately) {
    FakeUpdateDeletionPort port;
    port.unlink_results = {UpdateDeleteResult::permission_denied,
                           UpdateDeleteResult::success};
    UpdateDeletionService deletion(port);

    REQUIRE(!deletion.remove("/sd/firmware.bin"));

    REQUIRE_EQ(port.unlink_paths.size(), 1U);
    REQUIRE_EQ(port.attribute_paths.size(), 1U);
    REQUIRE_EQ(port.mode_paths.size(), 1U);
    REQUIRE(port.delays.empty());
}

TEST_CASE(upd_061_read_only_filesystem_stops_when_fat_adjustment_fails) {
    FakeUpdateDeletionPort port;
    port.unlink_results = {UpdateDeleteResult::read_only_filesystem,
                           UpdateDeleteResult::success};
    UpdateDeletionService deletion(port);

    REQUIRE(!deletion.remove("/sd/firmware.bin"));

    REQUIRE_EQ(port.unlink_paths.size(), 1U);
    REQUIRE_EQ(port.attribute_paths.size(), 1U);
    REQUIRE(port.mode_paths.empty());
}

TEST_CASE(upd_061_read_only_filesystem_retries_after_fat_adjustment) {
    FakeUpdateDeletionPort port;
    port.unlink_results = {UpdateDeleteResult::read_only_filesystem,
                           UpdateDeleteResult::success};
    port.attribute_clear_succeeds = true;
    UpdateDeletionService deletion(port);

    REQUIRE(deletion.remove("/sd/firmware.bin"));

    REQUIRE_EQ(port.unlink_paths.size(), 2U);
    REQUIRE(port.mode_paths.empty());
    REQUIRE_EQ(port.delays, std::vector<std::uint32_t>({100U, 200U}));
}

TEST_CASE(upd_063_final_failure_broadcasts_exact_unlimited_error) {
    FakeUpdateDeletionPort port;
    port.unlink_results = {UpdateDeleteResult::other_failure};
    UpdateDeletionService deletion(port);

    REQUIRE(!deletion.remove("/sd/lpc1768.bin"));

    REQUIRE_EQ(port.broadcast_types, std::vector<std::uint8_t>({0x90U}));
    REQUIRE_EQ(
        port.broadcasts,
        std::vector<std::string>({
            "Error: failed to delete [/sd/lpc1768.bin], please delete manually.\r\n"}));
}
