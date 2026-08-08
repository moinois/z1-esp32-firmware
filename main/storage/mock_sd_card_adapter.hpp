/** @file @brief Declares a PSRAM-backed FAT adapter for target-level SD simulation. */
#pragma once

#include "sd_storage_adapter.hpp"

#include <string>
#include <string_view>

namespace firmware::target {

/** Mounts a PSRAM-backed FAT volume while preserving the production SD interface. */
class MockSdCardAdapter final : public SdStorageAdapter {
public:
    /// Creates, formats, and mounts the volatile FAT volume at /sd.
    bool mount_for_boot() override;

    /// Reports the already-mounted simulated card without physical GPIO monitoring.
    void start() override;
};

/// Applies one test-only fault-control command when the mock is selected.
std::string handle_mock_sd_control(std::string_view command);

}  // namespace firmware::target
