// Implements play-state forwarding without coupling controllers to NVS.
#include "runtime_play_observer.hpp"

#include "runtime_counter_task.hpp"

#include <atomic>

namespace firmware::target {
namespace {
std::atomic_bool play_running{false};
std::atomic_bool controller_is_running{false};
}

void RuntimePlayObserver::play_state_changed(
    bool running, unsigned long long monotonic_milliseconds) const {
    play_running.store(running, std::memory_order_release);
    notify_runtime_play_state(running, monotonic_milliseconds);
}

bool streamed_play_running() {
    return play_running.load(std::memory_order_acquire);
}

bool controller_running() {
    return controller_is_running.load(std::memory_order_acquire);
}

void set_controller_running(bool running) {
    controller_is_running.store(running, std::memory_order_release);
}

}  // namespace firmware::target
