// Declares the target composition for aggregate firmware update processing.
#pragma once

namespace firmware::target {

// Starts one bounded update processing task after storage initialization.
class FirmwareUpdateAdapter {
public:
    // Creates the task; all update policy remains in application services.
    void start();
};

}  // namespace firmware::target
