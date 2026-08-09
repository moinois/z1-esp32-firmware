/** @file @brief Declares build-gated one-shot failures for production socket adapters. */
#pragma once

#include "network_fault_port.hpp"

#include <string>
#include <string_view>

namespace firmware::target {

/// Selects, clears, or reports one pending target socket failure.
std::string handle_mock_network_control(std::string_view command);

}  // namespace firmware::target
