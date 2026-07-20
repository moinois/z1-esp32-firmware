// Declares the exact path allow-list used by preview open requests.
#pragma once

#include <string_view>

namespace firmware::core {

// Reports whether a path is the videos directory or a safe descendant.
bool preview_path_allowed(std::string_view path);

}  // namespace firmware::core
