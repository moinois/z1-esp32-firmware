/** @file @brief Declares exact controller-path diagnostic formatting. */
#pragma once

#include <cstdint>
#include <string>

namespace firmware::application {

/// Formats the DIAG-038 warning emitted when the controller output FIFO is full.
std::string controller_queue_full_diagnostic(std::uint8_t frame_type);

}  // namespace firmware::application
