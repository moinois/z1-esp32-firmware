/** @file @brief Exact SD path allow-list for preview-open requests. */
#pragma once

#include <string_view>

namespace firmware::core {

/** Reports whether @p path is the videos directory or a safe descendant.
 *  This policy is a semantic allow-list in addition to general SD sandboxing.
 */
bool preview_path_allowed(std::string_view path);

}  // namespace firmware::core
