/** @file @brief Declares ordered retry policy for target CAN transmissions. */
#pragma once

#include "core/can/canopen_node.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>

namespace firmware::application {

/** Isolates the pending-message policy from TWAI and the RTOS clock. */
class CanPendingTransmitPort {
public:
    virtual ~CanPendingTransmitPort() = default;

    /// Attempts one bus transmission within the supplied deadline.
    virtual bool attempt(const core::CanFrame& frame,
                         std::uint32_t timeout_milliseconds) = 0;

    /// Adds the required post-attempt pacing interval.
    virtual void delay(std::uint32_t milliseconds) = 0;
};

/** Retains failed CAN messages and retries them before later messages. */
class CanPendingTransmitter {
public:
    explicit CanPendingTransmitter(CanPendingTransmitPort& port);

    /// Retains a message and drains in FIFO order until one attempt fails.
    void offer(const core::CanFrame& frame);

    /// Reports messages still awaiting a successful bus transmission.
    std::size_t pending() const;

private:
    CanPendingTransmitPort& port_;
    std::deque<core::CanFrame> pending_;
};

}  // namespace firmware::application
