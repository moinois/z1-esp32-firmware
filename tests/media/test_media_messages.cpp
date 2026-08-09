// Verifies compact, ordered media preemption JSON responses.
#include "test.hpp"

#include "core/media/media_messages.hpp"

#include <string_view>

using firmware::core::format_live_preemption;
using firmware::core::format_preview_preemption;

TEST_CASE(media_003_live_preemption_has_exact_compact_member_order) {
    REQUIRE_EQ(format_live_preemption("live"),
               std::string_view(
                   "{\"ns\":\"vlive\",\"rsp\":\"preempted\",\"reason\":\"live\","
                   "\"message\":\"The video stream channel is already occupied.\"}"));
}

TEST_CASE(media_004_preview_preemption_omits_empty_session_id) {
    REQUIRE_EQ(format_preview_preemption("preview", ""),
               std::string_view(
                   "{\"ns\":\"vpreview\",\"rsp\":\"preempted\","
                   "\"reason\":\"preview\",\"message\":\"The video stream channel is already occupied.\"}"));
    REQUIRE_EQ(format_preview_preemption("preview", "pv-123"),
               std::string_view(
                   "{\"ns\":\"vpreview\",\"rsp\":\"preempted\","
                   "\"reason\":\"preview\",\"message\":\"The video stream channel is already occupied.\","
                   "\"session_id\":\"pv-123\"}"));
}

TEST_CASE(media_005_preemption_strings_use_json_escaping_without_newline) {
    const auto response = format_live_preemption("a\"\\\n");
    REQUIRE_EQ(response,
               std::string_view(
                   "{\"ns\":\"vlive\",\"rsp\":\"preempted\",\"reason\":\"a\\\"\\\\\\n\","
                   "\"message\":\"The video stream channel is already occupied.\"}"));
}
