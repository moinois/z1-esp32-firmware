// Declares bounded FIFO policy for outgoing TCP frames.
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <deque>

namespace firmware::application {

class TcpTransmitQueue {
public:
    // Enqueues one nonempty frame no larger than the host protocol limit.
    bool enqueue(core::BytesView frame);

    // Returns the oldest complete frame without removing it.
    const core::ByteVector* front() const;

    // Removes the oldest frame after the transport fully sends it.
    void pop_front();

    // Reports current queued-item count.
    std::size_t size() const;

private:
    std::deque<core::ByteVector> frames_;
};

}  // namespace firmware::application
