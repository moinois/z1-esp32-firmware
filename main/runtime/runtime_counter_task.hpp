/** @file @brief Declares the target task that drives persistent runtime counters. */
#pragma once

#include <cstdint>

namespace firmware::target {

/** Owns initialization and periodic persistence scheduling for runtime counters. */
class RuntimeCounterTask {
public:
    /// Starts counter initialization and periodic power-on persistence.
    void start();
};

/// Requests immutable first-boot persistence from the running counter service.
void record_runtime_first_boot(std::int64_t unix_seconds);

/// Forwards play start/stop state to runtime accounting.
void notify_runtime_play_state(bool running, std::uint64_t monotonic_milliseconds);

/// Requests an immediate power-on counter persistence from the active service.
void request_runtime_persistence(std::uint64_t monotonic_milliseconds);

}  // namespace firmware::target
