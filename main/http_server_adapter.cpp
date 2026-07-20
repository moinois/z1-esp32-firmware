// Implements exact ESP-IDF HTTP listener configuration and nonfatal startup.
#include "http_server_adapter.hpp"

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firmware/application/web_config.hpp"
#include "firmware/application/static_file_server.hpp"
#include "firmware/application/preview_socket_input.hpp"
#include "firmware/core/web_static.hpp"
#include "firmware_update_adapter.hpp"
#include "ota_update_adapter.hpp"
#include "firmware/core/multipart_extractor.hpp"
#include "firmware/core/multipart_policy.hpp"
#include "firmware/application/direct_application_update.hpp"

#include <cstdio>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::target {
namespace {

constexpr char tag[] = "HTTP";

class DirectHttpOtaPort final
    : public firmware::application::DirectApplicationUpdatePort {
public:
    explicit DirectHttpOtaPort(httpd_req_t* request) : request_(request) {}

    // Stops camera ownership before changing the bootable application image.
    bool deinitialize_camera() override { return true; }
    // Selects the inactive application partition as the update destination.
    bool select_inactive_partition() override {
        return ota_.select_inactive_partition();
    }
    // Clears the destination partition before streaming the new image.
    bool erase_partition() override { return ota_.erase_inactive_partition(); }
    // Starts the ESP-IDF OTA write transaction.
    bool begin_update(std::size_t size) override {
        return ota_.begin_mainboard_write(static_cast<std::uint32_t>(size));
    }
    bool write_image(firmware::core::BytesView image) override {
        return ota_.write_mainboard(image);
    }
    // Completes and validates the ESP-IDF OTA write transaction.
    bool finish_update() override { return ota_.finalize_mainboard_write(); }
    // Marks the freshly written partition as the next boot target.
    bool select_boot_partition() override {
        return ota_.select_mainboard_for_boot();
    }
    // Aborts an in-progress OTA write after a validation or I/O failure.
    void abort_update() override { ota_.abort_mainboard_write(); }
    // Sends the service result through the active HTTP request.
    void send_response(std::uint16_t status, std::string_view body) override {
        const char* text = status == 200U ? "200 OK" : "500 Internal Server Error";
        httpd_resp_set_status(request_, text);
        httpd_resp_set_type(request_, "text/html");
        httpd_resp_send(request_, body.data(), body.size());
    }
    void delay_milliseconds(std::uint32_t milliseconds) override {
        vTaskDelay(pdMS_TO_TICKS(milliseconds));
    }
    // Reboots the controller after a successful direct update.
    void restart() override { esp_restart(); }

private:
    httpd_req_t* request_;
    firmware::target::OtaUpdateAdapter ota_;
};

// Reads SPIFFS files through the standard C file API used by VFS.
class StdioStaticFile final : public firmware::application::StaticFilePort {
public:
    std::optional<std::uint64_t> open(std::string_view path) override {
        file_ = std::fopen(std::string(path).c_str(), "rb");
        if (file_ == nullptr) {
            return std::nullopt;
        }
        return 0U;
    }

    std::optional<firmware::core::ByteVector> read(
        std::size_t maximum_bytes) override {
        if (file_ == nullptr) {
            return std::nullopt;
        }
        firmware::core::ByteVector bytes(maximum_bytes);
        const std::size_t count = std::fread(bytes.data(), 1U, maximum_bytes, file_);
        if (std::ferror(file_) != 0) {
            return std::nullopt;
        }
        bytes.resize(count);
        return bytes;
    }

    void close() override {
        if (file_ != nullptr) {
            std::fclose(file_);
            file_ = nullptr;
        }
    }

private:
    std::FILE* file_ = nullptr;
};

// Translates portable static responses to ESP-IDF HTTP server calls.
class EspHttpStaticResponse final
    : public firmware::application::StaticFileResponsePort {
public:
    explicit EspHttpStaticResponse(httpd_req_t* request) : request_(request) {}

    // Maps the portable status code to the HTTP reason phrase required by IDF.
    static const char* status_text(std::uint16_t status) {
        switch (status) {
            case 400U:
                return "400 Bad Request";
            case 404U:
                return "404 Not Found";
            case 405U:
                return "405 Method Not Allowed";
            case 413U:
                return "413 Payload Too Large";
            case 500U:
                return "500 Internal Server Error";
            default:
                return "500 Internal Server Error";
        }
    }

    void send_error(std::uint16_t status, std::string_view content_type,
                    std::string_view body) override {
        httpd_resp_set_status(request_, status_text(status));
        httpd_resp_set_type(request_, std::string(content_type).c_str());
        httpd_resp_send(request_, body.data(), body.size());
    }

    void begin_chunked(std::string_view content_type) override {
        httpd_resp_set_type(request_, std::string(content_type).c_str());
    }

    void send_chunk(firmware::core::BytesView chunk) override {
        httpd_resp_send_chunk(request_, reinterpret_cast<const char*>(chunk.data()),
                              chunk.size());
    }

    void finish_chunks() override {
        httpd_resp_send_chunk(request_, nullptr, 0);
    }

private:
    httpd_req_t* request_;
};

// Serves a wildcard static-file request using the tested application policy.
esp_err_t static_file_handler(httpd_req_t* request) {
    StdioStaticFile file;
    EspHttpStaticResponse response(request);
    firmware::application::StaticFileServer server;
    server.serve(request->uri, file, response);
    return ESP_OK;
}

// Sends the immutable firmware identity payload for the public API endpoint.
esp_err_t firmware_info_handler(httpd_req_t* request) {
    httpd_resp_set_type(request, "application/json");
    const std::string_view payload = firmware::core::firmware_identity_json();
    return httpd_resp_send(request, payload.data(), payload.size());
}

// Extracts the first multipart image and applies it through the direct OTA service.
esp_err_t firmware_update_handler(httpd_req_t* request) {
    constexpr std::size_t maximum_request_body = 2U * 1024U * 1024U;
    const std::size_t header_length = httpd_req_get_hdr_value_len(request, "Content-Type");
    if (header_length == 0U || header_length >= firmware::core::web_update::content_type_capacity) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "Invalid multipart Content-Type", HTTPD_RESP_USE_STRLEN);
    }
    std::string content_type(header_length + 1U, '\0');
    if (httpd_req_get_hdr_value_str(request, "Content-Type", content_type.data(),
                                    content_type.size()) != ESP_OK) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "Invalid multipart Content-Type", HTTPD_RESP_USE_STRLEN);
    }
    content_type.resize(header_length);
    const auto boundary = firmware::core::parse_multipart_content_type(content_type);
    if (!boundary.has_value() || request->content_len > maximum_request_body) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "Invalid multipart request", HTTPD_RESP_USE_STRLEN);
    }
    firmware::core::MultipartPartExtractor extractor(*boundary);
    std::vector<std::uint8_t> block(1024U);
    std::size_t received = 0U;
    while (received < static_cast<std::size_t>(request->content_len)) {
        const std::size_t remaining = static_cast<std::size_t>(request->content_len) - received;
        const int count = httpd_req_recv(
            request, reinterpret_cast<char*>(block.data()),
            std::min(block.size(), remaining));
        if (count <= 0) {
            httpd_resp_set_status(request, "400 Bad Request");
            return httpd_resp_send(request, "Bad Request", HTTPD_RESP_USE_STRLEN);
        }
        received += static_cast<std::size_t>(count);
        const bool final_block = received == static_cast<std::size_t>(request->content_len);
        if (!extractor.feed({block.data(), static_cast<std::size_t>(count)}, final_block)) {
            httpd_resp_set_status(request, "400 Bad Request");
            return httpd_resp_send(request, "Invalid multipart request", HTTPD_RESP_USE_STRLEN);
        }
    }
    if (extractor.status() != firmware::core::MultipartExtractStatus::complete) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "Invalid multipart request", HTTPD_RESP_USE_STRLEN);
    }
    DirectHttpOtaPort port(request);
    firmware::application::DirectApplicationUpdateService service;
    static_cast<void>(service.apply(extractor.content(), port));
    return ESP_OK;
}

#if CONFIG_HTTPD_WS_SUPPORT
// Receives one video WebSocket frame and applies the shared message boundary.
esp_err_t video_websocket_handler(httpd_req_t* request) {
    httpd_ws_frame_t frame{};
    if (httpd_ws_recv_frame(request, &frame, 0U) != ESP_OK) return ESP_FAIL;
    if (frame.len == 0U) return ESP_OK;
    std::vector<std::uint8_t> payload(frame.len);
    frame.payload = payload.data();
    return httpd_ws_recv_frame(request, &frame, frame.len);
}

// Receives one preview WebSocket frame and delegates text parsing to the application.
esp_err_t preview_websocket_handler(httpd_req_t* request) {
    httpd_ws_frame_t frame{};
    if (httpd_ws_recv_frame(request, &frame, 0U) != ESP_OK) return ESP_FAIL;
    if (frame.type != HTTPD_WS_TYPE_TEXT || frame.len == 0U) return ESP_OK;
    std::vector<std::uint8_t> payload(frame.len);
    frame.payload = payload.data();
    if (httpd_ws_recv_frame(request, &frame, frame.len) != ESP_OK) return ESP_FAIL;
    const auto request_value = firmware::application::accept_preview_socket_message(
        firmware::application::PreviewSocketMessageType::text,
        firmware::core::BytesView(payload));
    (void)request_value;
    return ESP_OK;
}
#endif

// Registers the exact GET handlers available before update/camera adapters.
void register_main_handlers(httpd_handle_t handle) {
    static const httpd_uri_t firmware_info_uri{
        .uri = "/api/firmware/info",
        .method = HTTP_GET,
        .handler = firmware_info_handler,
        .user_ctx = nullptr,
    };
    static const httpd_uri_t static_file_uri{
        .uri = "/*",
        .method = HTTP_GET,
        .handler = static_file_handler,
        .user_ctx = nullptr,
    };
    static const httpd_uri_t firmware_update_uri{
        .uri = "/update",
        .method = HTTP_POST,
        .handler = firmware_update_handler,
        .user_ctx = nullptr,
    };
#if CONFIG_HTTPD_WS_SUPPORT
    static const httpd_uri_t video_websocket_uri{
        .uri = "/ws_video",
        .method = HTTP_GET,
        .handler = video_websocket_handler,
        .user_ctx = nullptr,
        .is_websocket = true,
    };
    static const httpd_uri_t preview_websocket_uri{
        .uri = "/ws_preview",
        .method = HTTP_GET,
        .handler = preview_websocket_handler,
        .user_ctx = nullptr,
        .is_websocket = true,
    };
#endif
    httpd_register_uri_handler(handle, &firmware_info_uri);
    httpd_register_uri_handler(handle, &static_file_uri);
    httpd_register_uri_handler(handle, &firmware_update_uri);
#if CONFIG_HTTPD_WS_SUPPORT
    httpd_register_uri_handler(handle, &video_websocket_uri);
    httpd_register_uri_handler(handle, &preview_websocket_uri);
#endif
}

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
    main_handle_ = nullptr;
    httpd_config_t main_config = make_config(
        firmware::application::main_http_server);
    if (httpd_start(&main_handle_, &main_config) != ESP_OK) {
        ESP_LOGW(tag, "main HTTP server did not start");
    } else {
        register_main_handlers(main_handle_);
    }

    video_handle_ = nullptr;
    httpd_config_t video_config = make_config(
        firmware::application::video_http_server);
    if (httpd_start(&video_handle_, &video_config) != ESP_OK) {
        ESP_LOGW(tag, "video HTTP server did not start");
    }
}

}  // namespace firmware::target
