// Declares the ESP32-S3 SDMMC adapter for the portable card lifecycle.
#pragma once

namespace firmware::target {

class SdCardAdapter {
public:
    // Configures card detect and performs the optional synchronous boot mount.
    bool mount_for_boot();

    // Starts card detection, debounce, and nonfatal mount monitoring.
    void start();
};

}  // namespace firmware::target
