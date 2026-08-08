/** @file @brief Declares normalized preview WebSocket JSON requests. */
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::application {

/// Identifies the six accepted preview commands.
enum class PreviewCommand {
    open,
    play,
    pause,
    resume,
    seek,
    stop,
};

/// Owns normalized values delivered to the preview session service.
struct PreviewRequest {
    PreviewCommand command;
    std::uint32_t sequence = 0U;
    std::string path;
    std::string session_id;
    std::uint64_t from_milliseconds = 0U;
    std::int64_t from_frame = -1;
    std::uint64_t time_milliseconds = 0U;
    std::int64_t frame = -1;
};

/// Parses one text JSON request and ignores trailing bytes after its value.
std::optional<PreviewRequest> parse_preview_request(core::BytesView input);

/// Offers a text-view convenience overload for WebSocket adapters and tests.
inline std::optional<PreviewRequest> parse_preview_request(
    std::string_view input) {
    return parse_preview_request(core::BytesView(input));
}

}  // namespace firmware::application
