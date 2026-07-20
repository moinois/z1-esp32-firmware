// Declares the ESP-IDF TCP control listener.
#pragma once

namespace firmware::target {

class TcpControlAdapter {
public:
    // Starts the IPv4 listener and bounded accept task.
    void start();
};

}  // namespace firmware::target
