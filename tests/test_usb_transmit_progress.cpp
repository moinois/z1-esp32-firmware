// Verifies USB no-progress timeout boundaries and reset behavior.
#include "test.hpp"

#include "firmware/application/usb_transmit_progress.hpp"

using firmware::application::UsbTransmitProgress;

TEST_CASE(usb_008_timeout_requires_more_than_500_ms_without_progress) {
    UsbTransmitProgress progress;
    progress.begin(1000U);
    REQUIRE(!progress.expired(1500U));
    REQUIRE(progress.expired(1501U));
}

TEST_CASE(usb_008_positive_progress_restarts_timeout_window) {
    UsbTransmitProgress progress;
    progress.begin(1000U);
    progress.record_progress(1400U);
    REQUIRE(!progress.expired(1900U));
    REQUIRE(progress.expired(1901U));
}

TEST_CASE(usb_008_clear_disables_timeout_after_completion_or_discard) {
    UsbTransmitProgress progress;
    progress.begin(1000U);
    progress.clear();
    REQUIRE(!progress.expired(100000U));
}

