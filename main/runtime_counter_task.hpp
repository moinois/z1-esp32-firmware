// Declares the target task that drives persistent runtime counters.
#pragma once

#include <cstdint>

namespace firmware::target {

class RuntimeCounterTask {
public:
    // Starts counter initialization and periodic power-on persistence.
    void start();
};

// Requests immutable first-boot persistence from the running counter service.
void record_runtime_first_boot(std::int64_t unix_seconds);

}  // namespace firmware::target
