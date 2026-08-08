/** @file @brief Declares the ESP32-S3 SDMMC adapter for the portable card lifecycle. */
#pragma once

#include "sd_storage_adapter.hpp"

namespace firmware::target {

/** Implements physical SDMMC detection, mount, unmount, and capacity queries. */
class SdCardAdapter final : public SdStorageAdapter {
public:
    /// Configures card detect and performs the optional synchronous boot mount.
    bool mount_for_boot() override;

    /// Starts card detection, debounce, and nonfatal mount monitoring.
    void start() override;
};

}  // namespace firmware::target
