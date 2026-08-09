/** @file @brief No-op NVS fault boundary used by live and release builds. */
#include "mock_nvs_fault_adapter.hpp"

namespace firmware::target {

bool nvs_open_fault_active() { return false; }
bool nvs_commit_fault_active() { return false; }

}  // namespace firmware::target
