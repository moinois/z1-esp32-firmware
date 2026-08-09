/** @file @brief No-op SD fault/control boundary for live and release builds. */
#include "mock_sd_card_adapter.hpp"

namespace firmware::target {

std::string handle_mock_sd_control(std::string_view) {
    return "mock-sd unavailable in live build\n";
}

}  // namespace firmware::target
