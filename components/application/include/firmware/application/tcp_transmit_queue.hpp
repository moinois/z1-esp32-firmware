/** @file @brief Bounded FIFO policy for outgoing TCP frames. */
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <deque>

namespace firmware::application {

/** Retains encoded frames until a session sender completes each front item. */
class TcpTransmitQueue {
public:
    /// Enqueues one nonempty frame no larger than the host protocol limit.
    bool enqueue(core::BytesView frame);

    /// Returns the oldest frame; valid until the next mutating queue operation.
    const core::ByteVector* front() const;

    /// Removes the oldest frame after the transport fully sends it.
    void pop_front();

    /// Reports the current queued-item count.
    std::size_t size() const;

private:
    std::deque<core::ByteVector> frames_;
};

}  // namespace firmware::application
