/** @file @brief Case-sensitive route selection for both firmware web servers. */
#pragma once

#include <string_view>

namespace firmware::core {

/** Logical route selected before target HTTP handler dispatch. */
enum class HttpRoute {
    none,
    camera_resolution,
    firmware_info,
    wifi_diagnostics,
    configuration,
    gcodes_collection,
    gcode_file,
    application_update,
    web_volume_update,
    static_file,
    video_websocket,
    preview_websocket,
};

/** Selects a main-server route from method and complete request URI. */
HttpRoute select_main_http_route(std::string_view method,
                                 std::string_view complete_uri);

/** Selects a video-server route from method and complete request URI. */
HttpRoute select_video_http_route(std::string_view method,
                                  std::string_view complete_uri);

}  // namespace firmware::core
