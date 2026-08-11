/** @file @brief Declares the ESP-IDF TCP control listener. */
#pragma once

#include <cstddef>

namespace firmware::core {
struct Frame;
}

namespace firmware::application {
class Router;
struct HostIdentity;
}

namespace firmware::target {

/** Owns TCP listener/client workers and bridges sockets to shared host routing. */
class TcpControlAdapter {
public:
    /// Starts the IPv4 listener and bounded accept task.
    void start();
};

/// Returns the active TCP client count used by discovery payloads.
std::size_t active_tcp_client_count();

/// Releases the shared streamed-play owner after controller termination.
void tcp_router_play_ownership_release();

/// Applies USB disconnect semantics to shared streamed-play ownership.
void tcp_router_usb_disconnected();

/// Queues one response frame for every currently connected TCP session.
void broadcast_tcp_frame(const firmware::core::Frame& frame);

/// Delivers a globally selected addressed frame to the current logical slot.
bool deliver_tcp_frame(const firmware::application::HostIdentity& destination,
                       const firmware::core::Frame& frame);

/// Delivers one globally selected broadcast to every current TCP session.
void deliver_broadcast_tcp_frame(const firmware::core::Frame& frame);

/// Exposes the shared ownership/router state to other host transports.
firmware::application::Router& shared_host_router();

/// Claims the single global M942 worker slot across all host transports.
bool claim_m942_worker();

/// Releases the global M942 worker slot after worker termination or failure.
void release_m942_worker();

}  // namespace firmware::target
