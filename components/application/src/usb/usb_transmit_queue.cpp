/** @file @brief Implements whole-frame USB transmit admission and FIFO ordering. */
#include "application/usb/usb_transmit_queue.hpp"

namespace firmware::application {

bool UsbTransmitQueue::enqueue(core::BytesView frame,
                               std::chrono::milliseconds maximum_wait) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (failed_ || frame.size() == 0U || frame.size() > maximum_frame_size) {
        return false;
    }
    if (!capacity_changed_.wait_for(lock, maximum_wait, [this] {
            return failed_ || frames_.size() < maximum_items;
        })) {
        frames_.clear();
        failed_ = true;
        capacity_changed_.notify_all();
        return false;
    }
    if (failed_) {
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
        capacity_changed_.notify_one();
    }
}

void UsbTransmitQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    frames_.clear();
    capacity_changed_.notify_all();
}

bool UsbTransmitQueue::failed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return failed_;
}

void UsbTransmitQueue::reset_failure() {
    std::lock_guard<std::mutex> lock(mutex_);
    failed_ = false;
}

std::size_t UsbTransmitQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return frames_.size();
}

}  // namespace firmware::application
