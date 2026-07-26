// Implements the fixed-capacity, whole-frame TCP transmit queue.
#include "firmware/application/tcp_transmit_queue.hpp"

namespace firmware::application {

bool TcpTransmitQueue::enqueue(core::BytesView frame) {
    constexpr std::size_t maximum_items = 32U;
    constexpr std::size_t maximum_frame_size = 8300U;
    if (frame.size() == 0U || frame.size() > maximum_frame_size
        || frames_.size() >= maximum_items) {
        return false;
    }
    frames_.emplace_back(frame.begin(), frame.end());
    return true;
}

const core::ByteVector* TcpTransmitQueue::front() const {
    return frames_.empty() ? nullptr : &frames_.front();
}

void TcpTransmitQueue::pop_front() {
    if (!frames_.empty()) {
        frames_.pop_front();
    }
}

std::size_t TcpTransmitQueue::size() const { return frames_.size(); }

}  // namespace firmware::application
