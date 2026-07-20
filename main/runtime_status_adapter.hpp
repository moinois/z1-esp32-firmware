// Declares target runtime status sources and shared controller snapshots.
#pragma once

#include "firmware/application/controller_snapshots.hpp"
#include "firmware/application/runtime_status.hpp"

namespace firmware::application {
class Router;
}

namespace firmware::target {

// Provides live target sources to the portable aggregate status composer.
class RuntimeStatusAdapter final
    : public firmware::application::AggregatedStatusPort {
public:
    // Binds status ownership and recording sources to the application router/state.
    explicit RuntimeStatusAdapter(firmware::application::Router& router);

    bool host_transfer_active() const override;
    bool recording_requested() const override;
    bool recording_active() const override;
    std::optional<firmware::application::SdCapacity> sd_capacity() const override;
    firmware::application::UpdateStatus update_status() const override;
    std::optional<std::int32_t> station_rssi() const override;

private:
    firmware::application::Router& router_;
};

// Returns the process-wide controller snapshot store used by TCP and UART tasks.
firmware::application::ControllerSnapshots& shared_controller_snapshots();

// Publishes the newest update phase for aggregate status composition.
void publish_runtime_update_phase(std::uint8_t phase);

}  // namespace firmware::target
