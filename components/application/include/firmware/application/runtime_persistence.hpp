// Defines the shared persistent namespace and keys for runtime accounting.
#pragma once

#include <string_view>

namespace firmware::application::runtime_persistence {

inline constexpr std::string_view name_space = "runtime";
inline constexpr std::string_view first_boot_key = "first_boot";
inline constexpr std::string_view power_on_seconds_key = "pon_s";
inline constexpr std::string_view machine_seconds_key = "mach_s";

}  // namespace firmware::application::runtime_persistence
