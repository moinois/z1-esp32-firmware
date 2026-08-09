/** @file @brief No-op NVS fault boundary used by live and release builds. */
#include "mock_nvs_fault_adapter.hpp"

namespace firmware::target {

bool nvs_open_fault_active() { return false; }
bool nvs_commit_fault_active() { return false; }
std::string handle_mock_nvs_control(std::string_view) {
    return "mock-nvs unavailable in live build\n";
}

}  // namespace firmware::target
