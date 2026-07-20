// Declares the ESP32-S3 SDMMC adapter for the portable card lifecycle.
#pragma once

namespace firmware::target {

class SdCardAdapter {
public:
    // Starts card detection, debounce, and nonfatal mount monitoring.
    void start();
};

}  // namespace firmware::target
