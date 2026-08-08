/** @file @brief Declares the ESP-IDF storage adapter and periodic retention task. */
#pragma once

namespace firmware::target {

/** Bridges recording retention policy to mounted-SD enumeration and deletion. */
class StorageRetentionAdapter {
public:
    /// Starts the immediate and periodic storage retention checks.
    void start();
};

}  // namespace firmware::target
