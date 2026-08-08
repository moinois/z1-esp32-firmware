/** @file @brief Implements bounded FIFO admission for serialized local commands. */
#include "firmware/application/local_command_queue.hpp"

#include "firmware/core/frame.hpp"

namespace firmware::application {

bool LocalCommandQueue::enqueue(const core::Frame& frame) {
    const core::ByteVector encoded = core::encode_frame(frame);
    if (encoded.empty() || encoded.size() > maximum_frame_size ||
        commands_.size() >= maximum_pending_commands) {
        return false;
    }
    commands_.push_back(frame);
    return true;
}

std::optional<core::Frame> LocalCommandQueue::dequeue() {
    if (commands_.empty()) {
        return std::nullopt;
    }
    core::Frame command = std::move(commands_.front());
    commands_.pop_front();
    return command;
}

std::size_t LocalCommandQueue::pending() const {
    return commands_.size();
}

}  // namespace firmware::application
