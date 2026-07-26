// Declares the target composition for aggregate firmware update processing.
#pragma once

#include <cstdint>

namespace firmware::target {

// Starts one bounded update processing task after storage initialization.
class FirmwareUpdateAdapter {
public:
    // Creates the task; all update policy remains in application services.
    void start();
};

// Requests one coalesced aggregate processing pass from a local command.
void request_firmware_update_processing();

// Reports controller-transfer outcomes to the update monitor task.
void notify_controller_transfer_completed(std::uint64_t now_milliseconds);
void notify_controller_transfer_failed();
void notify_controller_transfer_cancelled();
void notify_controller_transfer_timeout(bool qualifying);

}  // namespace firmware::target
