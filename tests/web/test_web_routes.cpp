// Verifies exact main/video URI route selection before HTTP adapter wiring.
#include "test.hpp"

#include "firmware/core/web_routes.hpp"

using firmware::core::HttpRoute;
using firmware::core::select_main_http_route;
using firmware::core::select_video_http_route;

TEST_CASE(web_003_main_routes_match_case_sensitive_paths_and_methods) {
    REQUIRE_EQ(select_main_http_route("POST", "/api/camera/resolution?x=1"),
               HttpRoute::camera_resolution);
    REQUIRE_EQ(select_main_http_route("GET", "/api/firmware/info"),
               HttpRoute::firmware_info);
    REQUIRE_EQ(select_main_http_route("GET", "/api/wifi/diagnostics?fresh=1"),
               HttpRoute::wifi_diagnostics);
    REQUIRE_EQ(select_main_http_route("POST", "/update"), HttpRoute::application_update);
    REQUIRE_EQ(select_main_http_route("POST", "/updateffs"), HttpRoute::web_volume_update);
    REQUIRE_EQ(select_main_http_route("GET", "/api/camera/resolution"),
               HttpRoute::static_file);
    REQUIRE_EQ(select_main_http_route("GET", "/any/path"), HttpRoute::static_file);
}

TEST_CASE(web_003_video_routes_expose_only_the_two_websockets) {
    REQUIRE_EQ(select_video_http_route("GET", "/ws_video"), HttpRoute::video_websocket);
    REQUIRE_EQ(select_video_http_route("GET", "/ws_preview"), HttpRoute::preview_websocket);
    REQUIRE_EQ(select_video_http_route("POST", "/ws_video"), HttpRoute::none);
    REQUIRE_EQ(select_video_http_route("GET", "/other"), HttpRoute::none);
}

TEST_CASE(web_008_route_matching_is_case_sensitive_and_preserves_unknown_methods) {
    REQUIRE_EQ(select_main_http_route("get", "/api/firmware/info"), HttpRoute::none);
    REQUIRE_EQ(select_main_http_route("DELETE", "/update"), HttpRoute::none);
    REQUIRE_EQ(select_main_http_route("PUT", "/unknown"), HttpRoute::none);
}
