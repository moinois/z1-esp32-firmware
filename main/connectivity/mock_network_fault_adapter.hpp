/** @file @brief Declares build-gated one-shot failures for production socket adapters. */
#pragma once

#include "application/connectivity/network_fault_injection.hpp"

#include <string>
#include <string_view>

namespace firmware::target {

/// Consumes one matching injected fault in a mock-enabled build.
bool consume_network_fault(firmware::application::NetworkFault expected);

/// Selects, clears, or reports one pending target socket failure.
std::string handle_mock_network_control(std::string_view command);

}  // namespace firmware::target
