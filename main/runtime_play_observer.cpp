// Implements play-state forwarding without coupling controllers to NVS.
#include "runtime_play_observer.hpp"

#include "runtime_counter_task.hpp"

namespace firmware::target {

void RuntimePlayObserver::play_state_changed(
    bool running, unsigned long long monotonic_milliseconds) const {
    notify_runtime_play_state(running, monotonic_milliseconds);
}

}  // namespace firmware::target
