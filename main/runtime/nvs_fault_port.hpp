/** @file @brief Declares the production NVS fault-state port. */
#pragma once

namespace firmware::target {

/// Returns whether the next namespace open should fail.
bool nvs_open_fault_active();

/// Returns whether the next NVS mutation should fail before commit.
bool nvs_commit_fault_active();

}  // namespace firmware::target
