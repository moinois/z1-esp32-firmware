// Defines deterministic selection between live and mock hardware adapters.
#pragma once

namespace firmware::application {

struct HardwareAdapterSelection {
    bool mock_all_hardware;
    bool mock_sd_hardware;

    // Selects the mock SD backend when either the global or specific switch is active.
    constexpr bool mock_sd() const {
        return mock_all_hardware || mock_sd_hardware;
    }
};

}  // namespace firmware::application
