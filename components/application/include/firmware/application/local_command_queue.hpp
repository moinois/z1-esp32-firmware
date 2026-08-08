/** @file @brief Bounded FIFO used to serialize local command handling. */
#pragma once

#include "firmware/core/frame.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <cstddef>
#include <deque>
#include <optional>

namespace firmware::application {

/** Stores bounded local commands so flash-using services execute serially. */
class LocalCommandQueue {
public:
    /// Encoded-size admission limit including the common framing envelope.
    static constexpr std::size_t maximum_frame_size =
        core::protocol::controller_maximum_item_size;
    static constexpr std::size_t maximum_pending_commands = 32U;

    /// Queues one non-empty frame when size and capacity limits permit it.
    bool enqueue(const core::Frame& frame);

    /// Removes and returns the oldest command, if available.
    std::optional<core::Frame> dequeue();

    /// Reports the number of commands waiting for serialized handling.
    std::size_t pending() const;

private:
    std::deque<core::Frame> commands_;
};

}  // namespace firmware::application
    /// Maximum work retained to bound memory and response latency.
