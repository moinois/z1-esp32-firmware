/** @file @brief Implements recoverable aggregate-update task initialization. */
#include "application/update/update_task_initialization.hpp"

namespace firmware::application {

UpdateTaskInitialization::UpdateTaskInitialization(
    UpdateTaskInitializationPort& port)
    : port_(port) {}

void UpdateTaskInitialization::boot() {
    static_cast<void>(try_initialize());
    request();
}

void UpdateTaskInitialization::request() {
    if (!available_.load(std::memory_order_acquire)) {
        port_.warn_not_started();
        if (!try_initialize()) {
            return;
        }
    }
    port_.trigger_processing();
}

bool UpdateTaskInitialization::try_initialize() {
    if (available_.load(std::memory_order_acquire)) {
        return true;
    }
    if (initializing_.exchange(true, std::memory_order_acquire)) {
        return false;
    }
    const bool started = port_.start_processing();
    if (started) {
        available_.store(true, std::memory_order_release);
        port_.processing_started();
    }
    initializing_.store(false, std::memory_order_release);
    return started;
}

}  // namespace firmware::application
