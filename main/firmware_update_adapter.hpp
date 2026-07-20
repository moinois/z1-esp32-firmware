// Declares the target composition for aggregate firmware update processing.
#pragma once

namespace firmware::target {

// Starts one bounded update processing task after storage initialization.
class FirmwareUpdateAdapter {
public:
    // Creates the task; all update policy remains in application services.
    void start();
};

// Requests one coalesced aggregate processing pass from a local command.
void request_firmware_update_processing();

}  // namespace firmware::target
