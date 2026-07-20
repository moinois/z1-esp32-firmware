// Declares compact JSON messages shared by live and preview media channels.
#pragma once

#include <string>
#include <string_view>

namespace firmware::core {

// Formats the exact live-channel preemption response.
std::string format_live_preemption(std::string_view reason);

// Formats the exact preview-channel preemption response and optional session.
std::string format_preview_preemption(std::string_view reason,
                                      std::string_view session_id);

}  // namespace firmware::core
