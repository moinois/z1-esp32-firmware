// Declares subsystem tags used to group settings in config.txt.
#pragma once

#include <string_view>

namespace firmware::application {

// Groups general mainboard settings under the MAINBOARD_ namespace.
inline constexpr std::string_view mainboard_configuration_tag = "MAINBOARD";

}  // namespace firmware::application
