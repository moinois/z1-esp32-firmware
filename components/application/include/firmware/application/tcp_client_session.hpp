// Declares state shared by one TCP connection's receive and transmit paths.
#pragma once

#include "firmware/application/ownership.hpp"
#include "firmware/application/tcp_transmit_queue.hpp"
#include "firmware/core/frame.hpp"

#include <functional>
#include <optional>
#include <mutex>

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

    // Atomically removes and returns the next encoded frame for a sender.
    std::optional<core::ByteVector> take_next_transmit_frame();

    // Sends the queued front frame and removes it only after completion.
    bool send_next_transmit_frame(
        const std::function<bool(core::BytesView)>& send_function);

    // Reports whether at least one encoded frame is waiting for transmission.
    bool has_pending_transmit_frame() const;

    // Returns this connection's ownership identity.
    const HostIdentity& identity() const;

private:
    HostIdentity identity_;
    core::StreamDecoder decoder_;
    TcpTransmitQueue transmit_queue_;
    mutable std::mutex transmit_mutex_;
};

}  // namespace firmware::application
