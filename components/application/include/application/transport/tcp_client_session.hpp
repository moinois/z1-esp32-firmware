/** @file @brief State shared by one TCP connection's RX and TX paths. */
#pragma once

#include "application/runtime/ownership.hpp"
#include "application/transport/tcp_transmit_queue.hpp"
#include "core/protocol/frame.hpp"

#include <functional>
#include <optional>
#include <mutex>

namespace firmware::application {

/** Owns stream decoding, stable identity, and output queue for one connection. */
class TcpClientSession {
public:
    /// Receives each decoded frame together with the stable connection identity.
    using FrameHandler = std::function<void(const HostIdentity&, const core::Frame&)>;
    using OutputHandler = std::function<bool(const core::Frame&)>;

    /// Creates a session with a stable identity and TCP stream recovery policy.
    explicit TcpClientSession(HostIdentity identity);

    /// Decodes arbitrary stream fragments and delivers every complete frame.
    void receive(core::BytesView bytes, const FrameHandler& handler);

    /// Encodes and queues one outgoing frame for whole-frame transmission.
    bool queue_frame(const core::Frame& frame);

    /// Replaces direct per-session admission with a target-wide scheduler.
    void set_output_handler(OutputHandler handler);

    /// Bypasses the optional global handler when its selected item is delivered.
    bool queue_frame_direct(const core::Frame& frame);

    /// Provides the bounded outgoing queue to the owning transport adapter.
    TcpTransmitQueue& transmit_queue();

    /// Atomically removes and returns the next encoded frame for a sender.
    std::optional<core::ByteVector> take_next_transmit_frame();

    /// Sends the front frame and removes it only after complete transmission.
    bool send_next_transmit_frame(
        const std::function<bool(core::BytesView)>& send_function);

    /// Reports whether at least one encoded frame awaits transmission.
    bool has_pending_transmit_frame() const;

    /// Returns this connection's immutable ownership identity.
    const HostIdentity& identity() const;

private:
    HostIdentity identity_;
    core::StreamDecoder decoder_;
    TcpTransmitQueue transmit_queue_;
    mutable std::mutex transmit_mutex_;
    OutputHandler output_handler_;
};

}  // namespace firmware::application
