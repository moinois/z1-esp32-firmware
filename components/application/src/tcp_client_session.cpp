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
    return transmit_queue_.enqueue(core::encode_frame(frame));
}

TcpTransmitQueue& TcpClientSession::transmit_queue() {
    return transmit_queue_;
}

const HostIdentity& TcpClientSession::identity() const {
    return identity_;
}

}  // namespace firmware::application
