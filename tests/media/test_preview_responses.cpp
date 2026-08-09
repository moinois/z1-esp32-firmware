// Verifies exact preview response fields, optional values, and conflict text.
#include "test.hpp"
#include "application/playback/preview_responses.hpp"

TEST_CASE(prev_020_basic_response_is_ordered_and_compact) {
    const auto response = firmware::application::format_preview_response(
        firmware::application::PreviewCommand::seek, 12U, 0, "pv-1", "ok");
    REQUIRE_EQ(response, std::string(
        "{\"ns\":\"vpreview\",\"rsp\":\"seek\",\"seq\":12,\"err\":0,\"session_id\":\"pv-1\",\"message\":\"ok\"}"));
}

TEST_CASE(prev_020_omits_empty_optional_fields_and_021_uses_exact_conflict) {
    const auto response = firmware::application::format_preview_response(
        firmware::application::PreviewCommand::pause, 0U, 500);
    REQUIRE_EQ(response, std::string(
        "{\"ns\":\"vpreview\",\"rsp\":\"pause\",\"seq\":0,\"err\":500}"));
    REQUIRE_EQ(firmware::application::format_preview_conflict(firmware::application::PreviewCommand::resume, 4U, "pv-x"),
               std::string("{\"ns\":\"vpreview\",\"rsp\":\"resume\",\"seq\":4,\"err\":409,\"session_id\":\"pv-x\",\"message\":\"This conversation is in conflict. Please try again.\"}"));
}
