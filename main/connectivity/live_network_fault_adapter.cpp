/** @file @brief No-op network fault boundary used by live and release builds. */
#include "network_fault_port.hpp"

namespace firmware::target {

bool consume_network_fault(firmware::application::NetworkFault) { return false; }

}  // namespace firmware::target
