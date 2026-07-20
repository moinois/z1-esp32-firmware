// Implements exact ESP-IDF HTTP listener configuration and nonfatal startup.
#include "http_server_adapter.hpp"

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include "firmware/application/web_config.hpp"

namespace firmware::target {
namespace {

constexpr char tag[] = "HTTP";

// Applies shared and server-specific portable policy to an ESP-IDF config.
httpd_config_t make_config(
    const firmware::application::HttpServerConfig& policy) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = policy.port;
    config.ctrl_port = policy.control_port;
    config.max_open_sockets = policy.maximum_open_sockets;
    config.backlog_conn = policy.backlog;
    config.recv_wait_timeout = policy.receive_wait_seconds;
    config.send_wait_timeout = policy.send_wait_seconds;
    config.lru_purge_enable = policy.lru_session_eviction;
    config.keep_alive_enable = policy.tcp_keepalive;
    config.uri_match_fn = policy.wildcard_uri_matching
                              ? httpd_uri_match_wildcard
                              : nullptr;
    return config;
}

}  // namespace

void HttpServerAdapter::start() {
    httpd_handle_t main_handle = nullptr;
    httpd_config_t main_config = make_config(
        firmware::application::main_http_server);
    if (httpd_start(&main_handle, &main_config) != ESP_OK) {
        ESP_LOGW(tag, "main HTTP server did not start");
    }

    httpd_handle_t video_handle = nullptr;
    httpd_config_t video_config = make_config(
        firmware::application::video_http_server);
    if (httpd_start(&video_handle, &video_config) != ESP_OK) {
        ESP_LOGW(tag, "video HTTP server did not start");
    }
}

}  // namespace firmware::target
