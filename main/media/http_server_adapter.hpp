/** @file @brief Declares ESP-IDF startup for the independent main and video HTTP servers. */
#pragma once

#include "esp_http_server.h"

namespace firmware::target {

/// Owns listener handles while keeping startup failures nonfatal to other services.
class HttpServerAdapter {
public:
    /// Starts both configured plaintext listeners and retains their handles.
    void start();

private:
    httpd_handle_t main_handle_ = nullptr;
    httpd_handle_t video_handle_ = nullptr;
};

}  // namespace firmware::target
