// Declares bounded FIFO storage for outgoing USB protocol frames.
#pragma once

#include "firmware/core/bytes.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <cstddef>
#include <deque>
#include <mutex>

namespace firmware::application {

// Retains complete frames until the USB transport confirms transmission.
class UsbTransmitQueue {
public:
    static constexpr std::size_t maximum_items = 30U;
    // USB carries host file-data frames, including an 8192-byte payload. Do
    // not reuse the smaller controller/UART limit here: doing so accepts the
    // file request but silently rejects the resulting download response.
    static constexpr std::size_t maximum_frame_size =
        core::protocol::host_maximum_frame_size;

    // Queues one non-empty frame when capacity and size limits permit it.
    bool enqueue(core::BytesView frame);

    // Returns the oldest frame without removing it.
    const core::ByteVector* front() const;

    // Removes the oldest frame after complete transmission.
    void pop_front();

    // Reports the number of queued frames.
    std::size_t size() const;

private:
    // Serializes producer callbacks and the transport consumer.
    mutable std::mutex mutex_;
    std::deque<core::ByteVector> frames_;
};

}  // namespace firmware::application
