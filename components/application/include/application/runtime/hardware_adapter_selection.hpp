/** @file @brief Deterministic selection between live and mock hardware. */
#pragma once

namespace firmware::application {

/** Build-selected hardware policy consumed by the central adapter factory. */
struct HardwareAdapterSelection {
    /// Enables every available mock without enumerating individual switches.
    bool mock_all_hardware;
    /// Selects the PSRAM-backed FAT SD substitute.
    bool mock_sd_hardware;
    /// Selects deterministic camera lifecycle and frame generation.
    bool mock_camera_hardware;
    /// Selects the deterministic controller channel substitute.
    bool mock_controller_hardware;
    /// Enables NVS boundary faults without replacing persistent storage.
    bool mock_nvs_hardware;
    /// Enables one-shot network faults while retaining the live stack.
    bool mock_network_hardware;

    /// Selects mock SD when the global or specific switch is active.
    constexpr bool mock_sd() const {
        return mock_all_hardware || mock_sd_hardware;
    }

    /// Selects mock camera when the global or specific switch is active.
    constexpr bool mock_camera() const {
        return mock_all_hardware || mock_camera_hardware;
    }

    /// Selects mock controller when the global or specific switch is active.
    constexpr bool mock_controller() const {
        return mock_all_hardware || mock_controller_hardware;
    }

    /// Enables controlled NVS faults without replacing flash storage.
    constexpr bool mock_nvs() const {
        return mock_all_hardware || mock_nvs_hardware;
    }

    /// Enables one-shot socket failures while retaining the live network stack.
    constexpr bool mock_network() const {
        return mock_all_hardware || mock_network_hardware;
    }
};

}  // namespace firmware::application
