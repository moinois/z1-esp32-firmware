// Verifies SD-card startup, debounce, transition, logging, and capacity policy.
#include "test.hpp"

#include "firmware/application/sd_card_lifecycle.hpp"

#include <optional>
#include <string>
#include <vector>

using firmware::application::SdCardLifecycle;
using firmware::application::SdCardPort;
using firmware::application::SdMountConfig;

namespace {

// Records card and logging operations while exposing a controllable detect pin.
class FakeSdCardPort final : public SdCardPort {
public:
    // Returns the current active-low detect state as logical insertion.
    bool card_inserted() override {
        ++sample_count;
        return inserted;
    }

    // Records an exact mount request and returns the configured result.
    bool mount(const SdMountConfig& config) override {
        mount_configs.push_back(config);
        events.emplace_back("mount");
        return mount_succeeds;
    }

    // Starts a log session after successful mounting.
    void start_logging() override {
        events.emplace_back("start_logging");
    }

    // Stops and drains logging before an unmount request.
    void stop_and_drain_logging() override {
        events.emplace_back("stop_logging");
    }

    // Records an unmount request and returns the configured result.
    bool unmount() override {
        events.emplace_back("unmount");
        return unmount_succeeds;
    }

    // Returns the configured total byte capacity.
    std::optional<std::uint64_t> total_bytes() override {
        return total;
    }

    // Returns the configured free byte capacity.
    std::optional<std::uint64_t> free_bytes() override {
        return free;
    }

    bool inserted = false;
    bool mount_succeeds = true;
    bool unmount_succeeds = true;
    std::optional<std::uint64_t> total = 0U;
    std::optional<std::uint64_t> free = 0U;
    std::size_t sample_count = 0U;
    std::vector<SdMountConfig> mount_configs;
    std::vector<std::string> events;
};

// Advances three scheduled samples and the delayed insertion action.
void accept_insertion(SdCardLifecycle& lifecycle, FakeSdCardPort& port,
                      std::uint64_t first_sample) {
    lifecycle.poll(first_sample, port);
    lifecycle.poll(first_sample + 200U, port);
    lifecycle.poll(first_sample + 400U, port);
    lifecycle.poll(first_sample + 500U, port);
}

}  // namespace

TEST_CASE(sd_001_and_002_startup_samples_once_and_mounts_with_exact_policy) {
    FakeSdCardPort port;
    port.inserted = true;
    SdCardLifecycle lifecycle;

    lifecycle.start(0U, port);

    REQUIRE_EQ(port.sample_count, 1U);
    REQUIRE_EQ(port.mount_configs.size(), 1U);
    REQUIRE_EQ(port.mount_configs[0].mount_path, std::string_view("/sd"));
    REQUIRE(!port.mount_configs[0].format_if_mount_fails);
    REQUIRE_EQ(port.mount_configs[0].maximum_open_files, 16U);
    REQUIRE_EQ(port.mount_configs[0].allocation_unit_size, 16U * 1024U);
    REQUIRE(lifecycle.mounted());
    REQUIRE_EQ(port.events,
               std::vector<std::string>({"mount", "start_logging"}));
}

TEST_CASE(sd_002_mount_failure_is_nonfatal_and_does_not_start_logging) {
    FakeSdCardPort port;
    port.inserted = true;
    port.mount_succeeds = false;
    SdCardLifecycle lifecycle;

    lifecycle.start(0U, port);

    REQUIRE(!lifecycle.mounted());
    REQUIRE_EQ(port.events, std::vector<std::string>({"mount"}));
}

TEST_CASE(sd_003_matching_sample_restarts_the_three_sample_debounce) {
    FakeSdCardPort port;
    SdCardLifecycle lifecycle;
    lifecycle.start(0U, port);

    port.inserted = true;
    lifecycle.poll(200U, port);
    lifecycle.poll(400U, port);
    port.inserted = false;
    lifecycle.poll(600U, port);
    port.inserted = true;
    lifecycle.poll(800U, port);
    lifecycle.poll(1000U, port);
    lifecycle.poll(1200U, port);

    REQUIRE(port.mount_configs.empty());
    lifecycle.poll(1299U, port);
    REQUIRE(port.mount_configs.empty());
    lifecycle.poll(1300U, port);
    REQUIRE_EQ(port.mount_configs.size(), 1U);
}

TEST_CASE(sd_004_and_007_removal_stops_logging_before_immediate_unmount) {
    FakeSdCardPort port;
    SdCardLifecycle lifecycle;
    lifecycle.start(0U, port);
    port.inserted = true;
    accept_insertion(lifecycle, port, 200U);
    REQUIRE(lifecycle.mounted());

    port.events.clear();
    port.inserted = false;
    lifecycle.poll(800U, port);
    lifecycle.poll(1000U, port);
    lifecycle.poll(1200U, port);

    REQUIRE_EQ(port.events,
               std::vector<std::string>({"stop_logging", "unmount"}));
    REQUIRE(!lifecycle.mounted());
}

TEST_CASE(sd_005_present_at_boot_causes_one_monitored_retry_only_when_needed) {
    FakeSdCardPort successful_port;
    successful_port.inserted = true;
    SdCardLifecycle successful;
    successful.start(0U, successful_port);
    accept_insertion(successful, successful_port, 200U);
    REQUIRE_EQ(successful_port.mount_configs.size(), 1U);

    FakeSdCardPort retry_port;
    retry_port.inserted = true;
    retry_port.mount_succeeds = false;
    SdCardLifecycle retry;
    retry.start(0U, retry_port);
    accept_insertion(retry, retry_port, 200U);
    REQUIRE_EQ(retry_port.mount_configs.size(), 2U);
}

TEST_CASE(sd_006_failed_unmount_keeps_mounted_state_but_logging_stopped) {
    FakeSdCardPort port;
    SdCardLifecycle lifecycle;
    lifecycle.start(0U, port);
    port.inserted = true;
    accept_insertion(lifecycle, port, 200U);
    port.unmount_succeeds = false;

    port.inserted = false;
    lifecycle.poll(800U, port);
    lifecycle.poll(1000U, port);
    lifecycle.poll(1200U, port);
    lifecycle.poll(1400U, port);

    REQUIRE(lifecycle.mounted());
    REQUIRE_EQ(port.events.back(), std::string("unmount"));
}

TEST_CASE(sd_008_capacity_truncates_both_values_to_whole_mebibytes) {
    FakeSdCardPort port;
    port.total = 10U * 1024U * 1024U + 999U;
    port.free = 3U * 1024U * 1024U + 1023U;

    const auto capacity = SdCardLifecycle::read_capacity(port);

    REQUIRE(capacity.has_value());
    REQUIRE_EQ(capacity->total_mib, 10U);
    REQUIRE_EQ(capacity->free_mib, 3U);

    port.free = std::nullopt;
    REQUIRE(!SdCardLifecycle::read_capacity(port).has_value());
}
