/** @file @brief No-op network fault boundary used by live and release builds. */
#include "mock_network_fault_adapter.hpp"

namespace firmware::target {

bool consume_network_fault(firmware::application::NetworkFault) { return false; }

}  // namespace firmware::target
