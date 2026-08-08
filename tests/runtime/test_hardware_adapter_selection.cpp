// Verifies the centralized compile-time hardware adapter selection policy.
#include "test.hpp"

#include "firmware/application/hardware_adapter_selection.hpp"

using firmware::application::HardwareAdapterSelection;

TEST_CASE(hardware_adapter_selection_defaults_to_live_sd) {
    const HardwareAdapterSelection selection{false, false, false, false, false};

    REQUIRE(!selection.mock_sd());
    REQUIRE(!selection.mock_camera());
    REQUIRE(!selection.mock_controller());
    REQUIRE(!selection.mock_nvs());
}

TEST_CASE(hardware_adapter_selection_supports_specific_sd_mock) {
    const HardwareAdapterSelection selection{false, true, false, false, false};

    REQUIRE(selection.mock_sd());
}

TEST_CASE(hardware_adapter_selection_global_mock_includes_sd) {
    const HardwareAdapterSelection selection{true, false, false, false, false};

    REQUIRE(selection.mock_sd());
    REQUIRE(selection.mock_camera());
    REQUIRE(selection.mock_controller());
    REQUIRE(selection.mock_nvs());
}

TEST_CASE(hardware_adapter_selection_supports_specific_camera_mock) {
    const HardwareAdapterSelection selection{false, false, true, false, false};

    REQUIRE(!selection.mock_sd());
    REQUIRE(selection.mock_camera());
}

TEST_CASE(hardware_adapter_selection_supports_specific_controller_mock) {
    const HardwareAdapterSelection selection{false, false, false, true, false};

    REQUIRE(!selection.mock_sd());
    REQUIRE(!selection.mock_camera());
    REQUIRE(selection.mock_controller());
}

TEST_CASE(hardware_adapter_selection_supports_specific_nvs_faults) {
    const HardwareAdapterSelection selection{false, false, false, false, true};

    REQUIRE(!selection.mock_sd());
    REQUIRE(!selection.mock_controller());
    REQUIRE(selection.mock_nvs());
}
