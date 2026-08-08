/** @file @brief Declares the common lifecycle surface for live and mock SD storage adapters. */
#pragma once

namespace firmware::target {

/** Common lifecycle and capacity interface implemented by live and mock SD. */
class SdStorageAdapter {
public:
    virtual ~SdStorageAdapter() = default;

    /// Performs the optional synchronous boot mount.
    virtual bool mount_for_boot() = 0;

    /// Starts nonfatal background storage monitoring.
    virtual void start() = 0;
};

}  // namespace firmware::target
