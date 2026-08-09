/** @file @brief Declares the preview WebSocket input admission boundary. */
#pragma once

#include "application/playback/preview_request.hpp"

#include <optional>

namespace firmware::application {

/** WebSocket message category used to reject non-command preview payloads. */
enum class PreviewSocketMessageType { text, binary };

/// Ignores empty/non-text frames and parses only accepted text requests.
std::optional<PreviewRequest> accept_preview_socket_message(
    PreviewSocketMessageType type, core::BytesView payload);

}  // namespace firmware::application
