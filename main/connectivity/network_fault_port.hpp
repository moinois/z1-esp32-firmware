/** @file @brief Declares the transport-neutral one-shot network fault port. */
#pragma once

#include "application/connectivity/network_fault_injection.hpp"

namespace firmware::target {

/// Consumes one matching fault; live builds always return false.
bool consume_network_fault(firmware::application::NetworkFault expected);

}  // namespace firmware::target
