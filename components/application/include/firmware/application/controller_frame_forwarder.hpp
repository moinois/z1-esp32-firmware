// Declares a bounded encoder-backed bridge from host frames to controller output.
#pragma once

#include "firmware/application/controller_link.hpp"

namespace firmware::application {

class ControllerFrameForwarder {
public:
    // Queues one encoded host frame for controller UART transmission.
    bool forward(const core::Frame& frame);

    // Takes the next frame when the controller's spacing policy permits it.
    std::optional<core::ByteVector> take_ready(std::uint64_t now_milliseconds);

    // Reports how many complete encoded frames are waiting.
    std::size_t pending() const;

private:
    ControllerOutputQueue queue_;
};

}  // namespace firmware::application
