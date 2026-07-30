// Verifies the centralized compile-time hardware adapter selection policy.
#include "test.hpp"

#include "firmware/application/hardware_adapter_selection.hpp"

using firmware::application::HardwareAdapterSelection;

TEST_CASE(hardware_adapter_selection_defaults_to_live_sd) {
    const HardwareAdapterSelection selection{false, false};

    REQUIRE(!selection.mock_sd());
}

TEST_CASE(hardware_adapter_selection_supports_specific_sd_mock) {
    const HardwareAdapterSelection selection{false, true};

    REQUIRE(selection.mock_sd());
}

TEST_CASE(hardware_adapter_selection_global_mock_includes_sd) {
    const HardwareAdapterSelection selection{true, false};

    REQUIRE(selection.mock_sd());
}
