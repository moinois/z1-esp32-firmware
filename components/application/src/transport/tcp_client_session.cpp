// Implements per-connection framing and outgoing queue composition.
#include "firmware/application/tcp_client_session.hpp"

namespace firmware::application {

TcpClientSession::TcpClientSession(HostIdentity identity)
    : identity_(identity), decoder_(core::StreamPolicy::tcp()) {}

void TcpClientSession::receive(core::BytesView bytes,
                               const FrameHandler& handler) {
    if (!handler) {
        return;
    }
    for (const core::Frame& frame : decoder_.push(bytes)) {
        handler(identity_, frame);
    }
}

bool TcpClientSession::queue_frame(const core::Frame& frame) {
    std::lock_guard<std::mutex> lock(transmit_mutex_);
    return transmit_queue_.enqueue(core::encode_frame(frame));
}

TcpTransmitQueue& TcpClientSession::transmit_queue() {
    return transmit_queue_;
}

std::optional<core::ByteVector> TcpClientSession::take_next_transmit_frame() {
    std::lock_guard<std::mutex> lock(transmit_mutex_);
    const auto* frame = transmit_queue_.front();
    if (frame == nullptr) {
        return std::nullopt;
    }
    core::ByteVector copy = *frame;
    transmit_queue_.pop_front();
    return copy;
}

bool TcpClientSession::send_next_transmit_frame(
    const std::function<bool(core::BytesView)>& send_function) {
    std::lock_guard<std::mutex> lock(transmit_mutex_);
    const auto* frame = transmit_queue_.front();
    if (frame == nullptr) {
        return true;
    }
    if (!send_function || !send_function(*frame)) {
        return false;
    }
    transmit_queue_.pop_front();
    return true;
}

bool TcpClientSession::has_pending_transmit_frame() const {
    std::lock_guard<std::mutex> lock(transmit_mutex_);
    return transmit_queue_.front() != nullptr;
}

const HostIdentity& TcpClientSession::identity() const {
    return identity_;
}

}  // namespace firmware::application
