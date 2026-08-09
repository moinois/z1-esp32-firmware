/** @file @brief No-op network fault boundary used by live and release builds. */
#include "mock_network_fault_adapter.hpp"

namespace firmware::target {

bool consume_network_fault(firmware::application::NetworkFault) { return false; }
std::string handle_mock_network_control(std::string_view) {
    return "mock-net unavailable in live build\n";
}

}  // namespace firmware::target
