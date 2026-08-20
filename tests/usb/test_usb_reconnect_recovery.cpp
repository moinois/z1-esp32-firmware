/** @file @brief Tests USB logical reconnect timing and cancellation policy. */
#include "application/usb/usb_reconnect_recovery.hpp"
#include "test.hpp"

using firmware::application::UsbReconnectAction;
using firmware::application::UsbReconnectRecovery;
using firmware::application::usb_reconnect_initial_delay_milliseconds;
using firmware::application::usb_reconnect_low_hold_milliseconds;
using firmware::application::usb_reconnect_retry_delay_milliseconds;

TEST_CASE(usb_reconnect_recovery_cycles_bvalid_until_mount) {
    UsbReconnectRecovery recovery;
    constexpr std::uint64_t started = 500U;

    recovery.connection_lost(started);
    REQUIRE(recovery.pending());
    REQUIRE_EQ(recovery.poll(started), UsbReconnectAction::none);
    REQUIRE_EQ(recovery.poll(started + usb_reconnect_initial_delay_milliseconds),
               UsbReconnectAction::drive_bvalid_low);
    REQUIRE_EQ(recovery.poll(started + usb_reconnect_initial_delay_milliseconds +
                             usb_reconnect_low_hold_milliseconds - 1U),
               UsbReconnectAction::none);
    const std::uint64_t high_at =
        started + usb_reconnect_initial_delay_milliseconds +
        usb_reconnect_low_hold_milliseconds;
    REQUIRE_EQ(recovery.poll(high_at),
               UsbReconnectAction::drive_bvalid_high);
    REQUIRE_EQ(recovery.poll(high_at + usb_reconnect_retry_delay_milliseconds),
               UsbReconnectAction::drive_bvalid_low);

    recovery.connection_restored();
    REQUIRE(!recovery.pending());
    REQUIRE_EQ(recovery.poll(high_at + 10000U), UsbReconnectAction::none);
}

TEST_CASE(usb_reconnect_recovery_does_not_postpone_an_armed_cycle) {
    UsbReconnectRecovery recovery;
    recovery.connection_lost(100U);
    recovery.connection_lost(900U);

    REQUIRE_EQ(recovery.poll(100U + usb_reconnect_initial_delay_milliseconds),
               UsbReconnectAction::drive_bvalid_low);
}

TEST_CASE(usb_reconnect_recovery_can_be_rearmed_after_resume) {
    UsbReconnectRecovery recovery;
    recovery.connection_lost(100U);
    recovery.connection_restored();
    recovery.connection_lost(1000U);

    REQUIRE_EQ(recovery.poll(1000U + usb_reconnect_initial_delay_milliseconds),
               UsbReconnectAction::drive_bvalid_low);
}
