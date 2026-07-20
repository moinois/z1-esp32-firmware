// Verifies ordered preview metadata fields, filename extraction, and timing.
#include "test.hpp"
#include "firmware/application/preview_metadata.hpp"

TEST_CASE(prev_013_and_014_metadata_contains_avi_fields) {
    firmware::core::AviPreview avi;
    avi.frame_period_us = 40000U;
    avi.width = 640U;
    avi.height = 480U;
    avi.entries.resize(25U);
    const auto json = firmware::application::format_preview_metadata(
        avi, "pv-12345678-abcdef01", "/sd/videos/test.avi", 7U, 3U);
    REQUIRE_EQ(json, std::string("{\"ns\":\"vpreview\",\"rsp\":\"meta\",\"seq\":7,\"err\":0,\"session_id\":\"pv-12345678-abcdef01\",\"path\":\"/sd/videos/test.avi\",\"filename\":\"test.avi\",\"total_frames\":25,\"fps\":25,\"frame_period_us\":40000,\"duration_ms\":1000,\"width\":640,\"height\":480,\"first_frame_index\":3,\"stream\":\"jpeg\"}"));
}
