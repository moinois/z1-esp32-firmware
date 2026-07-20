// Declares state shared by one TCP connection's receive and transmit paths.
#pragma once

#include "firmware/application/ownership.hpp"
#include "firmware/application/tcp_transmit_queue.hpp"
#include "firmware/core/frame.hpp"

#include <functional>

namespace firmware::application {

class TcpClientSession {
public:
    using FrameHandler = std::function<void(const HostIdentity&, const core::Frame&)>;

    // Creates a session with a stable connection identity and TCP decoder policy.
    explicit TcpClientSession(HostIdentity identity);

    // Decodes received bytes and delivers every complete frame to the handler.
    void receive(core::BytesView bytes, const FrameHandler& handler);

    // Encodes and queues one outgoing frame for later whole-frame transmission.
    bool queue_frame(const core::Frame& frame);

    // Provides the session's bounded outgoing queue to the transport adapter.
    TcpTransmitQueue& transmit_queue();

    // Returns this connection's ownership identity.
    const HostIdentity& identity() const;

private:
    HostIdentity identity_;
    core::StreamDecoder decoder_;
    TcpTransmitQueue transmit_queue_;
};

}  // namespace firmware::application
