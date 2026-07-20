// Implements the WebSocket message-type and empty-payload boundary.
#include "firmware/application/preview_socket_input.hpp"

namespace firmware::application {

std::optional<PreviewRequest> accept_preview_socket_message(
    PreviewSocketMessageType type, core::BytesView payload) {
    if (type != PreviewSocketMessageType::text || payload.size() == 0U) {
        return std::nullopt;
    }
    return parse_preview_request(payload);
}

}  // namespace firmware::application
