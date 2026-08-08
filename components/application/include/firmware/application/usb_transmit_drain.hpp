/** @file @brief Target-independent draining of queued USB endpoint frames. */
#pragma once

#include "firmware/application/usb_transmit_progress.hpp"
#include "firmware/application/usb_transmit_queue.hpp"
#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <cstdint>

namespace firmware::application {

/** Abstracts the TinyUSB write FIFO for deterministic testing and substitution. */
class UsbTransmitDrainPort {
public:
    /// Enables safe destruction through a substituted endpoint adapter.
    virtual ~UsbTransmitDrainPort() = default;

    /// Reports how many bytes the endpoint can currently accept without blocking.
    virtual std::size_t available() = 0;

    /// Attempts one bounded write and returns the accepted prefix length.
    virtual std::size_t write(core::BytesView bytes) = 0;

    /// Requests immediate transport of all bytes accepted so far.
    virtual void flush() = 0;
};

/** Retains partial-frame offset and timeout state across endpoint polls. */
class UsbTransmitDrain {
public:
    /// Binds one queue to replaceable physical endpoint operations.
    UsbTransmitDrain(UsbTransmitQueue& queue, UsbTransmitDrainPort& port);

    /** Performs one nonblocking poll at a supplied monotonic timestamp.
     *  A stalled front frame is discarded only after the no-progress deadline;
     *  a disconnect resets progress tracking but preserves the queue itself.
     */
    void process(bool can_send, std::uint64_t monotonic_milliseconds);

private:
    /// Clears in-flight state without removing the retained queued frame.
    void clear_tracking();

    UsbTransmitQueue& queue_;
    UsbTransmitDrainPort& port_;
    UsbTransmitProgress progress_;
    const core::ByteVector* tracked_frame_ = nullptr;
    std::size_t transmitted_ = 0U;
};

}  // namespace firmware::application
