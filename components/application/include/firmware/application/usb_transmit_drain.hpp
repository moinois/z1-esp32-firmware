// Declares target-independent draining of queued USB frames to an endpoint.
#pragma once

#include "firmware/application/usb_transmit_progress.hpp"
#include "firmware/application/usb_transmit_queue.hpp"
#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <cstdint>

namespace firmware::application {

// Abstracts the TinyUSB write FIFO for deterministic fault injection.
class UsbTransmitDrainPort {
public:
    // Enables safe destruction through a substituted endpoint adapter.
    virtual ~UsbTransmitDrainPort() = default;

    // Reports how many bytes the endpoint can currently accept.
    virtual std::size_t available() = 0;

    // Attempts one bounded write and returns the accepted byte count.
    virtual std::size_t write(core::BytesView bytes) = 0;

    // Requests immediate transport of all bytes accepted so far.
    virtual void flush() = 0;
};

// Retains partial-frame offset and timeout state across transport polls.
class UsbTransmitDrain {
public:
    // Binds one queue to the replaceable physical endpoint operations.
    UsbTransmitDrain(UsbTransmitQueue& queue, UsbTransmitDrainPort& port);

    // Performs one nonblocking poll at the supplied monotonic time.
    void process(bool can_send, std::uint64_t monotonic_milliseconds);

private:
    // Clears in-flight state without removing the retained queued frame.
    void clear_tracking();

    UsbTransmitQueue& queue_;
    UsbTransmitDrainPort& port_;
    UsbTransmitProgress progress_;
    const core::ByteVector* tracked_frame_ = nullptr;
    std::size_t transmitted_ = 0U;
};

}  // namespace firmware::application
