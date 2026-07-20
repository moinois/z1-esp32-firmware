// Declares the target task that drives persistent runtime counters.
#pragma once

namespace firmware::target {

class RuntimeCounterTask {
public:
    // Starts counter initialization and periodic power-on persistence.
    void start();
};

}  // namespace firmware::target
