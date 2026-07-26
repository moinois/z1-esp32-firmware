// Declares the shared bounded capacity for runtime and serial operations.
#pragma once

#include <cstdint>

namespace firmware::target {

// Waits for one of the eight operation slots for at most the given duration.
bool admit_runtime_operation(std::uint32_t wait_milliseconds);

// Returns one previously admitted operation slot.
void complete_runtime_operation();

}  // namespace firmware::target
