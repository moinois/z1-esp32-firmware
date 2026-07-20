// Declares the ESP-IDF TWAI adapter for portable classic-CAN frames.
#pragma once

#include "firmware/core/canopen_node.hpp"

namespace firmware::target {

// Owns driver installation and bounded nonblocking CAN frame I/O.
class CanTwaiAdapter {
public:
    // Installs and starts TWAI controller zero with the complete fixed policy.
    bool initialize();

    // Receives one queued standard data frame without waiting.
    bool receive(core::CanFrame& frame) const;

    // Queues one standard data frame without waiting for transmit capacity.
    bool transmit(const core::CanFrame& frame) const;

    // Stops and uninstalls a driver after outer startup cannot continue.
    void shutdown() const;
};

}  // namespace firmware::target
