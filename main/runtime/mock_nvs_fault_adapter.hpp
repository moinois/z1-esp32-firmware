/** @file @brief Declares build-gated NVS boundary failure injection for HIL validation. */
#pragma once

#include "nvs_fault_port.hpp"

#include <string>
#include <string_view>

namespace firmware::target {

/// Selects or clears one fault when the NVS mock switch is enabled.
std::string handle_mock_nvs_control(std::string_view command);

}  // namespace firmware::target
