// Declares the target play-state observer used by controller adapters.
#pragma once

namespace firmware::target {

class RuntimePlayObserver {
public:
    // Forwards a play running transition with the supplied monotonic timestamp.
    void play_state_changed(bool running, unsigned long long monotonic_milliseconds) const;
};

// Reports whether streamed play is currently active for media tasks.
bool streamed_play_running();

}  // namespace firmware::target
