/** @file @brief Compact JSON messages shared by live and preview channels. */
#pragma once

#include <string>
#include <string_view>

namespace firmware::core {

/** Formats the exact live-channel preemption response with JSON escaping. */
std::string format_live_preemption(std::string_view reason);

/** Formats a preview preemption response and optional retained session ID. */
std::string format_preview_preemption(std::string_view reason,
                                      std::string_view session_id);

}  // namespace firmware::core
