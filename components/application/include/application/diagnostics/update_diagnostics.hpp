/** @file @brief Declares exact aggregate-update diagnostic formatting. */
#pragma once

#include "core/update/update_package.hpp"

#include <string>
#include <vector>

namespace firmware::application {

/// Formats the ordered DIAG-030 records for an already validated header.
std::vector<std::string> aggregate_header_diagnostics(
    const core::UpdateHeader& header, core::BytesView encoded_header);

}  // namespace firmware::application
