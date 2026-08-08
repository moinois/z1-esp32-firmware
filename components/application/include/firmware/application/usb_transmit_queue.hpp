/** @file @brief Thread-safe bounded FIFO for outgoing USB protocol frames. */
#pragma once

#include "firmware/core/bytes.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <cstddef>
#include <deque>
#include <mutex>

namespace firmware::application {

/** Retains complete frames until the USB endpoint confirms full transmission. */
class UsbTransmitQueue {
public:
    /// Maximum complete frames retained across all concurrent producers.
    static constexpr std::size_t maximum_items = 30U;
    /** Maximum encoded host frame, deliberately larger than the UART limit.
     *  USB carries 8192-byte file blocks; using the controller limit accepts a
     *  download request but silently prevents its data response from queuing.
     */
    static constexpr std::size_t maximum_frame_size =
        core::protocol::host_maximum_frame_size;

    /// Queues a non-empty complete frame when both limits permit it.
    bool enqueue(core::BytesView frame);

    /** Returns the oldest frame without removing it.
     *  The pointer remains valid until the next mutating queue operation.
     */
    const core::ByteVector* front() const;

    /// Removes the oldest frame after complete transmission or timeout discard.
    void pop_front();

    /// Reports the number of queued complete frames.
    std::size_t size() const;

private:
    /// Serializes producer callbacks and the single endpoint consumer.
    mutable std::mutex mutex_;
    std::deque<core::ByteVector> frames_;
};

}  // namespace firmware::application
