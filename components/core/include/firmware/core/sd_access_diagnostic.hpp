// Declares stable diagnostic reasons for failed access beneath the SD root.
#pragma once

#include <string_view>

namespace firmware::core {

// Classifies one failed SD operation using mount state before POSIX errno.
std::string_view sd_access_failure_reason(bool mounted, int error_number);

}  // namespace firmware::core
