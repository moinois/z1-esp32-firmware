// Declares compact JSON responses for preview command handling.
#pragma once

#include "firmware/application/preview_request.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {

// Formats one basic command response with the required field ordering.
std::string format_preview_response(PreviewCommand command,
                                    std::uint32_t sequence,
                                    std::int32_t error,
                                    std::string_view session_id = {},
                                    std::string_view message = {});

// Formats the standard response used when a session is not available.
std::string format_preview_conflict(PreviewCommand command,
                                    std::uint32_t sequence,
                                    std::string_view session_id = {});

}  // namespace firmware::application
