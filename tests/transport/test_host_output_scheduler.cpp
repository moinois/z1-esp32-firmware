// Verifies the global host-output capacity and selection rules in TRN-005/006.
#include "test.hpp"

#include "application/transport/host_output_scheduler.hpp"
#include "core/protocol/protocol_constants.hpp"

#include <chrono>
#include <thread>

using firmware::application::HostOutputAdmission;
using firmware::application::HostOutputDestination;
using firmware::application::HostOutputScheduler;
using firmware::application::HostOutputSource;
using firmware::core::Frame;

namespace {

Frame frame(std::uint8_t type, std::size_t payload_size = 1U) {
    return {type, firmware::core::ByteVector(payload_size, 0x5aU)};
}

const HostOutputDestination usb = HostOutputDestination::addressed(
    {firmware::application::HostTransport::usb, 0U, 0U});

}  // namespace

TEST_CASE(trn_005_limits_download_data_globally_and_independently) {
    HostOutputScheduler scheduler;
    scheduler.set_active_destinations(true, false);
    const Frame data = frame(firmware::core::protocol::file_data, 5U);
    for (std::size_t index = 0U; index < 32U; ++index) {
        REQUIRE_EQ(scheduler.admit(data, usb), HostOutputAdmission::accepted);
    }
    REQUIRE_EQ(scheduler.admit(data, usb), HostOutputAdmission::capacity_drop);
    REQUIRE_EQ(scheduler.pending_download_data(), 32U);

    REQUIRE_EQ(scheduler.admit(frame(0x91U), usb),
               HostOutputAdmission::accepted);
    REQUIRE_EQ(scheduler.pending_non_download(), 1U);
}

TEST_CASE(trn_005_treats_short_b3_as_non_download_output) {
    HostOutputScheduler scheduler;
    scheduler.set_active_destinations(true, false);
    REQUIRE_EQ(scheduler.admit(
                   frame(firmware::core::protocol::file_data, 4U), usb),
               HostOutputAdmission::accepted);
    REQUIRE_EQ(scheduler.pending_download_data(), 0U);
    REQUIRE_EQ(scheduler.pending_non_download(), 1U);
}

TEST_CASE(trn_006_limits_all_other_output_across_destinations) {
    HostOutputScheduler scheduler;
    scheduler.set_active_destinations(true, true);
    for (std::size_t index = 0U; index < 32U; ++index) {
        const auto destination = index % 2U == 0U
                                     ? usb
                                     : HostOutputDestination::broadcast();
        REQUIRE_EQ(scheduler.admit(frame(0x91U), destination),
                   HostOutputAdmission::accepted);
    }
    REQUIRE_EQ(scheduler.admit(frame(0x91U), usb),
               HostOutputAdmission::capacity_drop);
    REQUIRE_EQ(scheduler.pending_non_download(), 32U);
}

TEST_CASE(trn_006_catastrophic_source_purges_pending_non_download_output) {
    HostOutputScheduler scheduler;
    scheduler.set_active_destinations(true, false);
    for (std::size_t index = 0U; index < 32U; ++index) {
        REQUIRE_EQ(scheduler.admit(frame(0x91U), usb),
                   HostOutputAdmission::accepted);
    }

    REQUIRE_EQ(scheduler.admit(frame(0x92U), HostOutputDestination::broadcast(),
                               HostOutputSource::motion_board_unchanged),
               HostOutputAdmission::purged_at_capacity);
    REQUIRE_EQ(scheduler.pending_non_download(), 0U);
}

TEST_CASE(trn_006_no_active_host_purges_only_non_download_output) {
    HostOutputScheduler scheduler;
    scheduler.set_active_destinations(true, false);
    REQUIRE_EQ(scheduler.admit(frame(0x91U), usb),
               HostOutputAdmission::accepted);
    REQUIRE_EQ(scheduler.admit(
                   frame(firmware::core::protocol::file_data, 5U), usb),
               HostOutputAdmission::accepted);

    scheduler.set_active_destinations(false, false);
    REQUIRE_EQ(scheduler.pending_non_download(), 0U);
    REQUIRE_EQ(scheduler.pending_download_data(), 1U);
    REQUIRE_EQ(scheduler.admit(frame(0x91U), usb),
               HostOutputAdmission::capacity_drop);
}

TEST_CASE(trn_006_listing_may_wait_for_global_capacity) {
    HostOutputScheduler scheduler;
    scheduler.set_active_destinations(true, false);
    for (std::size_t index = 0U; index < 32U; ++index) {
        REQUIRE_EQ(scheduler.admit(frame(0x91U), usb),
                   HostOutputAdmission::accepted);
    }
    std::thread consumer([&scheduler] {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        static_cast<void>(scheduler.select());
    });
    REQUIRE_EQ(scheduler.admit_listing(frame(0x92U), usb,
                                       std::chrono::milliseconds(50)),
               HostOutputAdmission::accepted);
    consumer.join();
    REQUIRE_EQ(scheduler.pending_non_download(), 1U);
}

TEST_CASE(trn_006_selects_one_data_then_up_to_32_other_frames) {
    HostOutputScheduler scheduler;
    scheduler.set_active_destinations(true, true);
    REQUIRE_EQ(scheduler.admit(
                   frame(firmware::core::protocol::file_data, 5U), usb),
               HostOutputAdmission::accepted);
    for (std::uint8_t type = 1U; type <= 32U; ++type) {
        REQUIRE_EQ(scheduler.admit(frame(type), usb),
                   HostOutputAdmission::accepted);
    }

    const auto selection = scheduler.select();
    REQUIRE(selection.download_data.has_value());
    REQUIRE_EQ(selection.non_download.size(), 32U);
    REQUIRE(selection.delay_before_next_selection);
    REQUIRE_EQ(selection.non_download.front().frame.type, 1U);
    REQUIRE_EQ(selection.non_download.back().frame.type, 32U);
}

TEST_CASE(trn_006_only_non_download_batch_has_no_interval) {
    HostOutputScheduler scheduler;
    scheduler.set_active_destinations(true, false);
    REQUIRE_EQ(scheduler.admit(frame(0x91U), usb),
               HostOutputAdmission::accepted);
    auto selection = scheduler.select();
    REQUIRE(!selection.download_data.has_value());
    REQUIRE_EQ(selection.non_download.size(), 1U);
    REQUIRE(!selection.delay_before_next_selection);

    selection = scheduler.select();
    REQUIRE(!selection.download_data.has_value());
    REQUIRE(selection.non_download.empty());
    REQUIRE(selection.delay_before_next_selection);
}
