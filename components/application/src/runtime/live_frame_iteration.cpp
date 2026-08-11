/** @file @brief Implements LIVE-011/LIVE-012 live-frame operation ordering. */
#include "application/runtime/live_frame_iteration.hpp"

namespace firmware::application {

bool run_live_frame_iteration(std::uint32_t socket_id, LiveFramePort& port) {
    if (!port.socket_valid(socket_id)) return false;
    const auto frame = port.capture_jpeg();
    return frame.has_value() && port.send_jpeg(socket_id, *frame);
}

}  // namespace firmware::application
