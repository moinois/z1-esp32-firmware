/** @file @brief Declares the controller UART receive and command-dispatch task. */
#pragma once

#include "core/protocol/frame.hpp"

namespace firmware::target {

/** Polls the selected controller channel and dispatches decoded controller frames. */
class ControllerCommandLoop {
public:
    /// Starts UART initialization and the framed command loop.
    void start();
};

/// Queues a frame from another transport for serialized controller output.
bool enqueue_controller_frame(const firmware::core::Frame& frame);

/** Queues a mainboard-generated controller frame and emits DIAG-038 when the
 *  dedicated output capacity rejects it. Ordinary host forwarding deliberately
 *  uses enqueue_controller_frame() because ROUTE-018 requires silent drops. */
bool enqueue_generated_controller_frame(const firmware::core::Frame& frame);

/// Reports whether a controller transfer family currently suppresses traffic.
bool controller_firmware_transfer_active();
bool controller_configuration_transfer_active();
bool controller_factory_transfer_active();

}  // namespace firmware::target
