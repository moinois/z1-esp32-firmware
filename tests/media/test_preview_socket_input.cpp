// Verifies WebSocket text-only and empty-message admission.
#include "test.hpp"
#include "firmware/application/preview_socket_input.hpp"

TEST_CASE(prev_001_and_006_socket_input_accepts_text_only) {
    const auto request = firmware::application::accept_preview_socket_message(
        firmware::application::PreviewSocketMessageType::text,
        firmware::core::BytesView("{\"ns\":\"vpreview\",\"cmd\":\"open\"}"));
    REQUIRE(request.has_value());
    const auto binary = firmware::application::accept_preview_socket_message(
        firmware::application::PreviewSocketMessageType::binary,
        firmware::core::BytesView("{\"ns\":\"vpreview\",\"cmd\":\"open\"}"));
    REQUIRE(!binary.has_value());
    const auto empty = firmware::application::accept_preview_socket_message(
        firmware::application::PreviewSocketMessageType::text,
        firmware::core::BytesView());
    REQUIRE(!empty.has_value());
}
