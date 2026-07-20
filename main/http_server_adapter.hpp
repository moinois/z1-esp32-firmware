// Declares ESP-IDF startup for the independent main and video HTTP servers.
#pragma once

namespace firmware::target {

// Owns listener handles while keeping startup failures nonfatal to other services.
class HttpServerAdapter {
public:
    // Starts both configured plaintext listeners and retains their handles.
    void start();
};

}  // namespace firmware::target
