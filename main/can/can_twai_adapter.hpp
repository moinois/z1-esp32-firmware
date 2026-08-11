/** @file @brief Declares the ESP-IDF TWAI adapter for portable classic-CAN frames. */
#pragma once

#include "core/can/canopen_node.hpp"
#include "application/can/can_pending_transmitter.hpp"

#include <mutex>

namespace firmware::target {

/// Owns driver installation and bounded nonblocking CAN frame I/O.
class CanTwaiAdapter final : private application::CanPendingTransmitPort {
public:
    /// Installs and starts TWAI controller zero with the complete fixed policy.
    bool initialize();

    /// Receives one queued standard data frame without waiting.
    bool receive(core::CanFrame& frame) const;

    /// Retains one standard data frame until the pending-send policy succeeds.
    bool transmit(const core::CanFrame& frame);

    /// Samples the physical controller and returns the CANopen error register.
    std::uint8_t error_register() const;

    /// Stops and uninstalls a driver after outer startup cannot continue.
    void shutdown() const;

private:
    bool attempt(const core::CanFrame& frame,
                 std::uint32_t timeout_milliseconds) override;
    void delay(std::uint32_t milliseconds) override;

    std::mutex transmit_mutex_;
    application::CanPendingTransmitter transmitter_{*this};
};

}  // namespace firmware::target
