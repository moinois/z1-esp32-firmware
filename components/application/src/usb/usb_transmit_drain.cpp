/** @file @brief Implements partial USB writes, disconnect retention, and no-progress expiry. */
#include "application/usb/usb_transmit_drain.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace firmware::application {

UsbTransmitDrain::UsbTransmitDrain(UsbTransmitQueue& queue,
                                   UsbTransmitDrainPort& port)
    : queue_(queue), port_(port) {}

void UsbTransmitDrain::process(bool can_send,
                               std::uint64_t monotonic_milliseconds) {
    if (!can_send) {
        clear_tracking();
        return;
    }

    const core::ByteVector* frame = queue_.front();
    if (frame != tracked_frame_) {
        tracked_frame_ = frame;
        transmitted_ = 0U;
        if (frame != nullptr) {
            progress_.begin(monotonic_milliseconds);
        }
    }

    if (frame != nullptr && transmitted_ < frame->size()) {
        const std::size_t capacity = port_.available();
        if (capacity > 0U) {
            const std::size_t requested =
                std::min(capacity, frame->size() - transmitted_);
            const core::BytesView remainder(frame->data() + transmitted_,
                                            requested);
            const std::size_t written =
                std::min(port_.write(remainder), requested);
            if (written > 0U) {
                transmitted_ += written;
                // A short final fragment can otherwise remain buffered forever
                // when no later write arrives to trigger endpoint transmission.
                port_.flush();
                progress_.record_progress(monotonic_milliseconds);
            }
        }
        if (transmitted_ == frame->size()) {
            port_.flush();
            queue_.pop_front();
            clear_tracking();
            return;
        }
    }

    if (frame != nullptr && progress_.expired(monotonic_milliseconds)) {
        queue_.pop_front();
        clear_tracking();
    }
}

void UsbTransmitDrain::clear_tracking() {
    tracked_frame_ = nullptr;
    transmitted_ = 0U;
    progress_.clear();
}

}  // namespace firmware::application
