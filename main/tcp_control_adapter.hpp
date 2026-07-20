// Declares the ESP-IDF TCP control listener.
#pragma once

#include <cstddef>

namespace firmware::target {

class TcpControlAdapter {
public:
    // Starts the IPv4 listener and bounded accept task.
    void start();
};

// Returns the active TCP client count used by discovery payloads.
std::size_t active_tcp_client_count();

}  // namespace firmware::target
