/** @file @brief Implements frame encoding and delegation to the controller output queue. */
#include "firmware/application/controller_frame_forwarder.hpp"

namespace firmware::application {

bool ControllerFrameForwarder::forward(const core::Frame& frame) {
    return queue_.enqueue(core::encode_frame(frame));
}

std::optional<core::ByteVector> ControllerFrameForwarder::take_ready(
    std::uint64_t now_milliseconds) {
    return queue_.take_ready(now_milliseconds);
}

std::size_t ControllerFrameForwarder::pending() const {
    return queue_.pending();
}

}  // namespace firmware::application
