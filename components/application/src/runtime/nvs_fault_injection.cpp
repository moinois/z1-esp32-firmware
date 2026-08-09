/** @file @brief Implements thread-safe NVS boundary failure selection. */
#include "application/runtime/nvs_fault_injection.hpp"

namespace firmware::application {

void NvsFaultInjection::select(NvsFault fault) {
    fault_.store(fault, std::memory_order_release);
}

bool NvsFaultInjection::fail_open() const {
    return fault_.load(std::memory_order_acquire) == NvsFault::open;
}

bool NvsFaultInjection::fail_commit() const {
    return fault_.load(std::memory_order_acquire) == NvsFault::commit;
}

}  // namespace firmware::application
