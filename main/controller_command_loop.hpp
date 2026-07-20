// Declares the controller UART receive and command-dispatch task.
#pragma once

#include "firmware/core/frame.hpp"

namespace firmware::target {

class ControllerCommandLoop {
public:
    // Starts UART initialization and the framed command loop.
    void start();
};

// Queues a frame from another transport for serialized controller output.
bool enqueue_controller_frame(const firmware::core::Frame& frame);

}  // namespace firmware::target
