// Declares build-gated NVS boundary failure injection for HIL validation.
#pragma once

#include <string>
#include <string_view>

namespace firmware::target {

// Returns true when the next NVS namespace open must be rejected.
bool nvs_open_fault_active();

// Returns true when an NVS mutation must be rejected before commit.
bool nvs_commit_fault_active();

// Selects or clears one fault when the NVS mock switch is enabled.
std::string handle_mock_nvs_control(std::string_view command);

}  // namespace firmware::target
