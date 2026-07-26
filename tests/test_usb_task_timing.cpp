// Verifies that USB worker polling yields enough time for the idle watchdog.
#include "test.hpp"

#include "firmware/application/usb_task_timing.hpp"

TEST_CASE(usb_workers_use_a_watchdog_friendly_poll_period) {
    REQUIRE(firmware::application::usb_task_poll_delay_milliseconds >= 10U);
}
