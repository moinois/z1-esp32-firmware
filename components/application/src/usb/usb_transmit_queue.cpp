/** @file @brief Implements whole-frame USB transmit admission and FIFO ordering. */
#include "firmware/application/usb_transmit_queue.hpp"

namespace firmware::application {

bool UsbTransmitQueue::enqueue(core::BytesView frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frame.size() == 0U || frame.size() > maximum_frame_size ||
        frames_.size() >= maximum_items) {
        return false;
    }
    frames_.emplace_back(frame.begin(), frame.end());
    return true;
}

const core::ByteVector* UsbTransmitQueue::front() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return frames_.empty() ? nullptr : &frames_.front();
}

void UsbTransmitQueue::pop_front() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!frames_.empty()) {
        frames_.pop_front();
    }
}

std::size_t UsbTransmitQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return frames_.size();
}

}  // namespace firmware::application
