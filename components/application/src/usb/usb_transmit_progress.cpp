/** @file @brief Implements USB transmit progress timing with wrap-safe elapsed arithmetic. */
#include "application/usb/usb_transmit_progress.hpp"

namespace firmware::application {

void UsbTransmitProgress::begin(std::uint64_t monotonic_milliseconds) {
    active_ = true;
    last_progress_milliseconds_ = monotonic_milliseconds;
}

void UsbTransmitProgress::record_progress(
    std::uint64_t monotonic_milliseconds) {
    if (active_) {
        last_progress_milliseconds_ = monotonic_milliseconds;
    }
}

bool UsbTransmitProgress::expired(
    std::uint64_t monotonic_milliseconds) const {
    return active_ && monotonic_milliseconds >= last_progress_milliseconds_ &&
           monotonic_milliseconds - last_progress_milliseconds_ >
               no_progress_timeout_milliseconds;
}

void UsbTransmitProgress::clear() {
    active_ = false;
    last_progress_milliseconds_ = 0U;
}

}  // namespace firmware::application
