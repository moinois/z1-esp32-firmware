// Declares the ESP-IDF TCP control listener.
#pragma once

#include <cstddef>

namespace firmware::core {
struct Frame;
}

namespace firmware::application {
class Router;
}

namespace firmware::target {

class TcpControlAdapter {
public:
    // Starts the IPv4 listener and bounded accept task.
    void start();
};

// Returns the active TCP client count used by discovery payloads.
std::size_t active_tcp_client_count();

// Releases the shared streamed-play owner after controller termination.
void tcp_router_play_ownership_release();

// Applies USB disconnect semantics to shared streamed-play ownership.
void tcp_router_usb_disconnected();

// Queues one response frame for every currently connected TCP session.
void broadcast_tcp_frame(const firmware::core::Frame& frame);

// Exposes the shared ownership/router state to other host transports.
firmware::application::Router& shared_host_router();

}  // namespace firmware::target
