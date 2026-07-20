// Declares the controller UART receive and command-dispatch task.
#pragma once

namespace firmware::target {

class ControllerCommandLoop {
public:
    // Starts UART initialization and the framed command loop.
    void start();
};

}  // namespace firmware::target
