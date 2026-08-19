/** @file @brief Implements the BOOT-002/BOOT-003 persistent-store recovery policy. */
#include "application/runtime/persistent_store_initialization.hpp"

namespace firmware::application {

bool initialize_persistent_store(PersistentStoreInitializationPort& port) {
    auto result = port.initialize();
    if (result == PersistentStoreInitializationResult::success) return true;

    const bool exhausted =
        result == PersistentStoreInitializationResult::exhausted_pages ||
        result == PersistentStoreInitializationResult::incompatible_version;
    if (exhausted) {
        port.report_exhausted_recovery();
        // BOOT-002 makes failure of this first mandatory erase immediately fatal.
        if (!port.erase()) return false;
        result = port.initialize();
        if (result == PersistentStoreInitializationResult::success) return true;
    }

    port.report_general_recovery();
    // BOOT-003 requires the final initialization attempt even if this erase
    // request fails. The same final attempt completes BOOT-002 recovery.
    static_cast<void>(port.erase());
    return port.initialize() == PersistentStoreInitializationResult::success;
}

}  // namespace firmware::application
