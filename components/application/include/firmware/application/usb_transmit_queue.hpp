// Declares bounded FIFO storage for outgoing USB protocol frames.
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <deque>

namespace firmware::application {

// Retains complete frames until the USB transport confirms transmission.
class UsbTransmitQueue {
public:
    static constexpr std::size_t maximum_items = 30U;
    static constexpr std::size_t maximum_frame_size = 544U;

    // Queues one non-empty frame when capacity and size limits permit it.
    bool enqueue(core::BytesView frame);

    // Returns the oldest frame without removing it.
    const core::ByteVector* front() const;

    // Removes the oldest frame after complete transmission.
    void pop_front();

    // Reports the number of queued frames.
    std::size_t size() const;

private:
    std::deque<core::ByteVector> frames_;
};

}  // namespace firmware::application
