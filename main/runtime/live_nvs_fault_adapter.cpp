/** @file @brief No-op NVS fault boundary used by live and release builds. */
#include "nvs_fault_port.hpp"

namespace firmware::target {

bool nvs_open_fault_active() { return false; }
bool nvs_commit_fault_active() { return false; }

}  // namespace firmware::target
