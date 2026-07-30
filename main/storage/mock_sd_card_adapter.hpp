// Declares a PSRAM-backed FAT adapter for target-level SD simulation.
#pragma once

#include "sd_storage_adapter.hpp"

namespace firmware::target {

class MockSdCardAdapter final : public SdStorageAdapter {
public:
    // Creates, formats, and mounts the volatile FAT volume at /sd.
    bool mount_for_boot() override;

    // Reports the already-mounted simulated card without physical GPIO monitoring.
    void start() override;
};

}  // namespace firmware::target
