/** @file @brief Implements exact controller-path diagnostic formatting. */
#include "application/diagnostics/controller_diagnostics.hpp"

#include <cstdio>

namespace firmware::application {

std::string controller_queue_full_diagnostic(std::uint8_t frame_type) {
    char output[48]{};
    std::snprintf(output, sizeof(output),
                  "TxQueue full, drop frame type=0x%02X",
                  static_cast<unsigned>(frame_type));
    return output;
}

}  // namespace firmware::application
