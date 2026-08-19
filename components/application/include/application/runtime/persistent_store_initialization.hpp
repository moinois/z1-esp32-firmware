/** @file @brief Declares the BOOT-002/BOOT-003 persistent-store recovery policy. */
#pragma once

namespace firmware::application {

/// Portable classification of one platform persistent-store initialization result.
enum class PersistentStoreInitializationResult {
    success,
    exhausted_pages,
    incompatible_version,
    other_failure,
};

/// Isolates platform initialization, erase, and diagnostic effects.
class PersistentStoreInitializationPort {
public:
    virtual ~PersistentStoreInitializationPort() = default;

    /// Performs one initialization attempt and retains platform error detail.
    virtual PersistentStoreInitializationResult initialize() = 0;
    /// Requests one complete persistent-store erase.
    virtual bool erase() = 0;
    /// Emits the exhausted/incompatible recovery warning.
    virtual void report_exhausted_recovery() = 0;
    /// Emits the current initialization error and full-erase retry warning.
    virtual void report_general_recovery() = 0;
};

/// Runs the bounded recovery sequence before any other application service.
bool initialize_persistent_store(PersistentStoreInitializationPort& port);

}  // namespace firmware::application
