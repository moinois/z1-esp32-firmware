/** @file @brief Declares one ordered live-video validation/capture/send iteration. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstdint>
#include <optional>

namespace firmware::application {

/// Isolates live-frame ordering from the camera and ESP HTTP server APIs.
class LiveFramePort {
public:
    virtual ~LiveFramePort() = default;

    /// Reports whether the claimed socket identity is currently a WebSocket.
    virtual bool socket_valid(std::uint32_t socket_id) = 0;

    /// Captures one JPEG frame from the shared camera.
    virtual std::optional<core::ByteVector> capture_jpeg() = 0;

    /// Sends a captured frame without revalidating the socket identity.
    virtual bool send_jpeg(std::uint32_t socket_id, core::BytesView frame) = 0;
};

/** Runs one LIVE-012 iteration and reports whether streaming may continue.
 *
 * Validation deliberately occurs once before capture. The socket is not
 * checked again before send because replacement during capture is explicitly
 * observable under LIVE-012.
 */
bool run_live_frame_iteration(std::uint32_t socket_id, LiveFramePort& port);

}  // namespace firmware::application
