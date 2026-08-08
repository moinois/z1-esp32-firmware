/** @file @brief Declares the controller UART receive and command-dispatch task. */
#pragma once

#include "firmware/core/frame.hpp"

namespace firmware::target {

/** Polls the selected controller channel and dispatches decoded controller frames. */
class ControllerCommandLoop {
public:
    /// Starts UART initialization and the framed command loop.
    void start();
};

/// Queues a frame from another transport for serialized controller output.
bool enqueue_controller_frame(const firmware::core::Frame& frame);

/// Reports whether a controller transfer family currently suppresses traffic.
bool controller_firmware_transfer_active();
bool controller_configuration_transfer_active();
bool controller_factory_transfer_active();

}  // namespace firmware::target
