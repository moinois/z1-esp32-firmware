// Defines deterministic selection between live and mock hardware adapters.
#pragma once

namespace firmware::application {

struct HardwareAdapterSelection {
    bool mock_all_hardware;
    bool mock_sd_hardware;
    bool mock_camera_hardware;

    // Selects the mock SD backend when either the global or specific switch is active.
    constexpr bool mock_sd() const {
        return mock_all_hardware || mock_sd_hardware;
    }

    // Selects the mock camera when either the global or specific switch is active.
    constexpr bool mock_camera() const {
        return mock_all_hardware || mock_camera_hardware;
    }
};

}  // namespace firmware::application
