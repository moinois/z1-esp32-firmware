// Implements exact endpoint matching and GET-only static fallback behavior.
#include "firmware/core/web_routes.hpp"

#include "firmware/core/http_policy.hpp"

namespace firmware::core {
namespace {

constexpr std::string_view camera_resolution_path = "/api/camera/resolution";
constexpr std::string_view firmware_info_path = "/api/firmware/info";
constexpr std::string_view wifi_diagnostics_path = "/api/wifi/diagnostics";
constexpr std::string_view application_update_path = "/update";
constexpr std::string_view web_volume_update_path = "/updateffs";
constexpr std::string_view video_websocket_path = "/ws_video";
constexpr std::string_view preview_websocket_path = "/ws_preview";

}  // namespace

HttpRoute select_main_http_route(std::string_view method,
                                 std::string_view complete_uri) {
    const std::string_view path = http_uri_path(complete_uri);
    if (method == "POST") {
        if (path == camera_resolution_path) {
            return HttpRoute::camera_resolution;
        }
        if (path == application_update_path) {
            return HttpRoute::application_update;
        }
        if (path == web_volume_update_path) {
            return HttpRoute::web_volume_update;
        }
        return HttpRoute::none;
    }
    if (method == "GET") {
        if (path == firmware_info_path) {
            return HttpRoute::firmware_info;
        }
        if (path == wifi_diagnostics_path) {
            return HttpRoute::wifi_diagnostics;
        }
        return HttpRoute::static_file;
    }
    return HttpRoute::none;
}

HttpRoute select_video_http_route(std::string_view method,
                                  std::string_view complete_uri) {
    if (method != "GET") {
        return HttpRoute::none;
    }
    const std::string_view path = http_uri_path(complete_uri);
    if (path == video_websocket_path) {
        return HttpRoute::video_websocket;
    }
    if (path == preview_websocket_path) {
        return HttpRoute::preview_websocket;
    }
    return HttpRoute::none;
}

}  // namespace firmware::core
