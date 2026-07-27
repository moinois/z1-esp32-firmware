// Defines source names shared by configuration get and set commands.
#pragma once

#include <string_view>

namespace firmware::application::configuration_sources {

inline constexpr std::string_view cached = "cached";
inline constexpr std::string_view sd = "sd";
inline constexpr std::string_view live = "live";

}  // namespace firmware::application::configuration_sources
