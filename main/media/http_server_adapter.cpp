// Implements exact ESP-IDF HTTP listener configuration and nonfatal startup.
#include "http_server_adapter.hpp"

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_partition.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firmware/application/web_config.hpp"
#include "firmware/application/static_file_server.hpp"
#include "firmware/application/preview_socket_input.hpp"
#include "firmware/core/web_static.hpp"
#include "firmware/core/media_messages.hpp"
#include "firmware_update_adapter.hpp"
#include "ota_update_adapter.hpp"
#include "hardware_adapter_factory.hpp"
#include "camera_hardware_adapter.hpp"
#include "firmware/application/live_control_policy.hpp"
#include "firmware/application/preview_open.hpp"
#include "firmware/application/preview_metadata.hpp"
#include "firmware/application/preview_responses.hpp"
#include "firmware/application/preview_playback.hpp"
#include "firmware/application/preview_frame_step.hpp"
#include "firmware/core/preview_path_policy.hpp"
#include "firmware/core/sd_user_path.hpp"
#include "firmware/core/avi_preview.hpp"
#include "firmware/core/multipart_extractor.hpp"
#include "firmware/core/multipart_policy.hpp"
#include "firmware/core/wifi_statistics.hpp"
#include "firmware/application/direct_application_update.hpp"
#include "firmware/application/direct_web_volume_update.hpp"
#include "wlan_event_adapter.hpp"
#include "wifi_diagnostic_log.hpp"

#include <lwip/inet.h>
#include <lwip/sockets.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::target {
namespace {

constexpr char tag[] = "HTTP";
firmware::application::LiveControlPolicy live_control_policy;

#if CONFIG_HTTPD_WS_SUPPORT
// Retains the currently admitted preview file and playback state.
struct PreviewRuntime {
    firmware::core::ByteVector file;
    firmware::core::AviPreview avi;
    std::string path;
    std::string session_id;
    std::uint32_t socket_id = 0U;
    firmware::application::PreviewMode mode =
        firmware::application::PreviewMode::stopped;
    std::uint32_t current_frame = 0U;
};

std::optional<PreviewRuntime> preview_runtime;
std::atomic_uint32_t preview_generation{0U};
std::atomic_uint32_t live_generation{0U};

constexpr TickType_t live_frame_interval = pdMS_TO_TICKS(100U);

// Owns one continuous live-camera stream until ownership is revoked.
struct LiveStreamTaskContext {
    httpd_handle_t handle;
    int socket_id;
    std::uint32_t generation;
};

// Captures and sends one complete camera frame to an admitted live socket.
bool send_live_frame(httpd_handle_t handle, int socket_id) {
    const auto frame = HardwareAdapterFactory::camera().capture_jpeg();
    if (!frame.has_value()) {
        return false;
    }
    httpd_ws_frame_t outgoing{};
    outgoing.type = HTTPD_WS_TYPE_BINARY;
    outgoing.payload = const_cast<std::uint8_t*>(frame->data());
    outgoing.len = frame->size();
    return httpd_ws_send_frame_async(handle, socket_id, &outgoing) == ESP_OK;
}

// Captures and asynchronously sends JPEG frames without retaining a request pointer.
void live_stream_task(void* parameter) {
    auto* context = static_cast<LiveStreamTaskContext*>(parameter);
    while (live_generation.load(std::memory_order_acquire) ==
               context->generation &&
           httpd_ws_get_fd_info(context->handle, context->socket_id) ==
               HTTPD_WS_CLIENT_WEBSOCKET) {
        if (!send_live_frame(context->handle, context->socket_id)) {
            break;
        }
        vTaskDelay(live_frame_interval);
    }
    delete context;
    vTaskDelete(nullptr);
}

// Starts a generation-bound live stream for one admitted socket.
void start_live_stream(httpd_req_t* request) {
    const auto generation = live_generation.fetch_add(
                                1U, std::memory_order_acq_rel) +
                            1U;
    auto* context = new LiveStreamTaskContext{
        request->handle,
        httpd_req_to_sockfd(request),
        generation,
    };
    if (xTaskCreate(live_stream_task, "live_stream", 4096U, context, 4U,
                    nullptr) != pdPASS) {
        delete context;
    }
}

// Reads one preview file for the metadata admission path.
std::optional<firmware::core::ByteVector> read_preview_file(
    std::string_view path) {
    std::FILE* file = std::fopen(std::string(path).c_str(), "rb");
    if (file == nullptr || std::fseek(file, 0L, SEEK_END) != 0) {
        if (file != nullptr) {
            std::fclose(file);
        }
        return std::nullopt;
    }
    const long length = std::ftell(file);
    if (length < 0L || std::fseek(file, 0L, SEEK_SET) != 0) {
        std::fclose(file);
        return std::nullopt;
    }
    firmware::core::ByteVector content(static_cast<std::size_t>(length));
    const std::size_t read = std::fread(content.data(), 1U, content.size(), file);
    std::fclose(file);
    if (read != content.size()) {
        return std::nullopt;
    }
    return content;
}

// Sends one text message through the preview WebSocket.
esp_err_t send_preview_text(httpd_req_t* request, std::string_view text) {
    httpd_ws_frame_t response{};
    response.type = HTTPD_WS_TYPE_TEXT;
    response.payload = reinterpret_cast<uint8_t*>(const_cast<char*>(text.data()));
    response.len = text.size();
    return httpd_ws_send_frame(request, &response);
}

// Sends one indexed JPEG frame from the retained preview file.
bool send_preview_frame(httpd_req_t* request, const PreviewRuntime& runtime,
                        std::size_t frame_index) {
    const auto frame = firmware::core::read_avi_frame(
        firmware::core::BytesView(runtime.file), runtime.avi, frame_index,
        runtime.file.size());
    if (!frame.has_value()) {
        return false;
    }
    httpd_ws_frame_t response{};
    response.type = HTTPD_WS_TYPE_BINARY;
    response.payload = const_cast<std::uint8_t*>(frame->data());
    response.len = frame->size();
    return httpd_ws_send_frame(request, &response) == ESP_OK;
}

// Owns one asynchronous preview playback pass until stop or end-of-stream.
struct PreviewPlaybackTaskContext {
    httpd_handle_t handle;
    int socket_id;
    firmware::core::ByteVector file;
    firmware::core::AviPreview avi;
    std::size_t next_frame;
    std::uint32_t generation;
};

// Streams indexed frames asynchronously without retaining an HTTP request pointer.
void preview_playback_task(void* parameter) {
    auto* context = static_cast<PreviewPlaybackTaskContext*>(parameter);
    while (context->next_frame < context->avi.entries.size() &&
           preview_generation.load(std::memory_order_acquire) ==
               context->generation) {
        const bool connection_alive =
            httpd_ws_get_fd_info(context->handle, context->socket_id) ==
            HTTPD_WS_CLIENT_WEBSOCKET;
        const auto frame = firmware::core::read_avi_frame(
            firmware::core::BytesView(context->file), context->avi,
            context->next_frame, context->file.size());
        const bool read_succeeded = frame.has_value();
        bool send_succeeded = false;
        if (connection_alive && read_succeeded) {
            httpd_ws_frame_t response{};
            response.type = HTTPD_WS_TYPE_BINARY;
            response.payload = const_cast<std::uint8_t*>(frame->data());
            response.len = frame->size();
            send_succeeded = httpd_ws_send_frame_async(
                                 context->handle, context->socket_id, &response) == ESP_OK;
        }
        const auto step = firmware::application::schedule_preview_frame(
            context->next_frame, context->avi.entries.size(),
            context->avi.frame_period_us, read_succeeded, send_succeeded,
            connection_alive, false);
        if (step.action != firmware::application::PreviewFrameAction::send_frame) {
            break;
        }
        ++context->next_frame;
        vTaskDelay(pdMS_TO_TICKS(step.delay_milliseconds));
    }
    delete context;
    vTaskDelete(nullptr);
}

// Starts asynchronous playback after the command response's first frame.
void start_preview_playback_task(httpd_req_t* request,
                                 const PreviewRuntime& runtime) {
    auto* context = new PreviewPlaybackTaskContext{
        request->handle,
        httpd_req_to_sockfd(request),
        runtime.file,
        runtime.avi,
        static_cast<std::size_t>(runtime.current_frame) + 1U,
        preview_generation.load(std::memory_order_acquire),
    };
    if (xTaskCreate(preview_playback_task, "preview_play", 6144U, context,
                    4U, nullptr) != pdPASS) {
        delete context;
    }
}
#endif

class DirectHttpOtaPort final
    : public firmware::application::DirectApplicationUpdatePort {
public:
    explicit DirectHttpOtaPort(httpd_req_t* request) : request_(request) {}

    // Stops camera ownership before changing the bootable application image.
    bool deinitialize_camera() override {
        return firmware::target::HardwareAdapterFactory::camera().deinitialize();
    }
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
    void restart() override {
        ESP_LOGW(tag, "restart requested after direct application update");
        esp_restart();
    }

private:
    httpd_req_t* request_;
    firmware::target::OtaUpdateAdapter ota_;
};

// Bridges raw web-volume writes to the ESP-IDF SPIFFS data partition.
class DirectHttpWebVolumePort final
    : public firmware::application::DirectWebVolumeUpdatePort {
public:
    explicit DirectHttpWebVolumePort(httpd_req_t* request) : request_(request) {}

    // Locates the complete SPIFFS partition without unmounting it.
    std::optional<std::size_t> partition_size() override {
        partition_ = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
        if (partition_ == nullptr) {
            return std::nullopt;
        }
        return partition_->size;
    }

    // Erases every byte before offset-zero content is written.
    bool erase_partition() override {
        return partition_ != nullptr &&
               esp_partition_erase_range(partition_, 0U, partition_->size) == ESP_OK;
    }

    // Writes the extracted image at the beginning of the selected partition.
    bool write_content(firmware::core::BytesView content) override {
        if (partition_ == nullptr) {
            return false;
        }
        if (content.size() == 0U) {
            return true;
        }
        return esp_partition_write(partition_, 0U, content.data(), content.size()) == ESP_OK;
    }

    // Sends the portable update result through the HTTP request.
    void send_response(std::uint16_t status, std::string_view body) override {
        httpd_resp_set_status(request_,
                              status == 200U ? "200 OK" : "500 Internal Server Error");
        httpd_resp_set_type(request_, "text/html");
        httpd_resp_send(request_, body.data(), body.size());
    }

    // Delays in the FreeRTOS task before rebooting.
    void delay_milliseconds(std::uint32_t milliseconds) override {
        vTaskDelay(pdMS_TO_TICKS(milliseconds));
    }

    // Reboots after a successful web-volume replacement.
    void restart() override {
        ESP_LOGW(tag, "restart requested after web-volume update");
        esp_restart();
    }

private:
    httpd_req_t* request_;
    const esp_partition_t* partition_ = nullptr;
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
            case 200U:
                return "200 OK";
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

std::string_view wifi_authentication_name(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN: return "open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2-PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Enterprise";
        case WIFI_AUTH_WPA3_PSK: return "WPA3-PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3-PSK";
        case WIFI_AUTH_WAPI_PSK: return "WAPI-PSK";
        case WIFI_AUTH_OWE: return "OWE";
        default: return "unknown";
    }
}

// Returns a privacy-bounded live radio snapshot and boot-lifetime event counts.
esp_err_t wifi_diagnostics_handler(httpd_req_t* request) {
    firmware::core::WifiStatistics statistics;
    wifi_ap_record_t access_point{};
    if (esp_wifi_sta_get_ap_info(&access_point) == ESP_OK) {
        statistics.connected = true;
        statistics.rssi_dbm = access_point.rssi;
        statistics.channel = access_point.primary;
        statistics.authentication = wifi_authentication_name(access_point.authmode);
    }

    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info{};
    if (netif != nullptr && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK &&
        ip_info.ip.addr != 0U) {
        char address[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &ip_info.ip.addr, address, sizeof(address)) !=
            nullptr) {
            statistics.ipv4_address = address;
        }
    }

    const WifiEventStatistics events = wifi_event_statistics();
    statistics.station_starts = events.station_starts;
    statistics.associations = events.associations;
    statistics.disconnections = events.disconnections;
    statistics.addresses_acquired = events.addresses_acquired;
    statistics.addresses_lost = events.addresses_lost;
    statistics.last_disconnect_reason = events.last_disconnect_reason;
    statistics.recent_events = wifi_diagnostic_log().read();

    const std::string payload =
        firmware::core::format_wifi_statistics_json(statistics);
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, payload.data(), payload.size());
}

// Applies a bounded camera-resolution JSON request through the portable policy.
esp_err_t camera_resolution_handler(httpd_req_t* request) {
    constexpr std::size_t maximum_request_body = 63U;
    std::vector<std::uint8_t> body(maximum_request_body);
    const int count = httpd_req_recv(request, reinterpret_cast<char*>(body.data()),
                                     body.size());
    if (count < 0) {
        return ESP_FAIL;
    }
    firmware::application::CameraResolutionEndpoint endpoint;
    const auto response = endpoint.handle(
        firmware::core::BytesView(body.data(), static_cast<std::size_t>(count)),
        firmware::target::HardwareAdapterFactory::camera());
    httpd_resp_set_status(request,
                          EspHttpStaticResponse::status_text(response.status_code));
    httpd_resp_set_type(request, std::string(response.content_type).c_str());
    return httpd_resp_send(request, response.body.data(), response.body.size());
}

constexpr unsigned maximum_consecutive_receive_timeouts = 6U;
constexpr std::size_t upload_progress_interval = 256U * 1024U;

esp_err_t send_bad_request(httpd_req_t* request, const char* message) {
    httpd_resp_set_status(request, "400 Bad Request");
    return httpd_resp_send(request, message, HTTPD_RESP_USE_STRLEN);
}

// Receives and extracts one multipart part while tolerating bounded transport gaps.
std::unique_ptr<firmware::core::MultipartPartExtractor> receive_multipart_part(
    httpd_req_t* request, std::optional<std::size_t> maximum_request_body) {
    const std::size_t header_length =
        httpd_req_get_hdr_value_len(request, "Content-Type");
    if (header_length == 0U ||
        header_length >= firmware::core::web_update::content_type_capacity) {
        static_cast<void>(send_bad_request(
            request, "Invalid multipart Content-Type"));
        return nullptr;
    }
    std::string content_type(header_length + 1U, '\0');
    if (httpd_req_get_hdr_value_str(request, "Content-Type", content_type.data(),
                                    content_type.size()) != ESP_OK) {
        static_cast<void>(send_bad_request(
            request, "Invalid multipart Content-Type"));
        return nullptr;
    }
    content_type.resize(header_length);
    const auto boundary =
        firmware::core::parse_multipart_content_type(content_type);
    const auto request_size = static_cast<std::size_t>(request->content_len);
    if (!boundary.has_value() ||
        (maximum_request_body.has_value() &&
         request_size > *maximum_request_body)) {
        static_cast<void>(send_bad_request(request, "Invalid multipart request"));
        return nullptr;
    }

    auto extractor =
        std::make_unique<firmware::core::MultipartPartExtractor>(*boundary);
    std::vector<std::uint8_t> block(1024U);
    std::size_t received = 0U;
    std::size_t next_progress = upload_progress_interval;
    unsigned consecutive_timeouts = 0U;
    ESP_LOGI(tag, "multipart receive started: %u bytes",
             static_cast<unsigned>(request_size));
    while (received < request_size) {
        const std::size_t remaining = request_size - received;
        const int count = httpd_req_recv(
            request, reinterpret_cast<char*>(block.data()),
            std::min(block.size(), remaining));
        if (count == HTTPD_SOCK_ERR_TIMEOUT) {
            if (consecutive_timeouts < maximum_consecutive_receive_timeouts) {
                ++consecutive_timeouts;
                ESP_LOGW(tag,
                         "multipart receive timeout %u/%u after %u/%u bytes",
                         consecutive_timeouts,
                         maximum_consecutive_receive_timeouts,
                         static_cast<unsigned>(received),
                         static_cast<unsigned>(request_size));
                continue;
            }
            ESP_LOGE(tag,
                     "multipart timeout budget exhausted after %u retries at %u/%u bytes",
                     maximum_consecutive_receive_timeouts,
                     static_cast<unsigned>(received),
                     static_cast<unsigned>(request_size));
        }
        if (count <= 0) {
            ESP_LOGE(tag, "multipart receive failed: result=%d after %u/%u bytes",
                     count, static_cast<unsigned>(received),
                     static_cast<unsigned>(request_size));
            static_cast<void>(send_bad_request(request, "Bad Request"));
            return nullptr;
        }
        consecutive_timeouts = 0U;
        received += static_cast<std::size_t>(count);
        if (received >= next_progress || received == request_size) {
            ESP_LOGI(tag, "multipart receive progress: %u/%u bytes",
                     static_cast<unsigned>(received),
                     static_cast<unsigned>(request_size));
            next_progress = received + upload_progress_interval;
        }
        if (!extractor->feed(
                {block.data(), static_cast<std::size_t>(count)},
                received == request_size)) {
            static_cast<void>(send_bad_request(request,
                                               "Invalid multipart request"));
            return nullptr;
        }
    }
    if (extractor->status() !=
        firmware::core::MultipartExtractStatus::complete) {
        static_cast<void>(send_bad_request(request, "Invalid multipart request"));
        return nullptr;
    }
    ESP_LOGI(tag, "multipart receive completed: %u bytes",
             static_cast<unsigned>(received));
    return extractor;
}

// Extracts the first multipart image and applies it through the direct OTA service.
esp_err_t firmware_update_handler(httpd_req_t* request) {
    constexpr std::size_t maximum_request_body = 2U * 1024U * 1024U;
    auto extractor = receive_multipart_part(request, maximum_request_body);
    if (extractor == nullptr) {
        return ESP_OK;
    }
    DirectHttpOtaPort port(request);
    firmware::application::DirectApplicationUpdateService service;
    static_cast<void>(service.apply(extractor->content(), port));
    return ESP_OK;
}

// Extracts the first multipart part and applies it as a raw web-volume image.
esp_err_t web_volume_update_handler(httpd_req_t* request) {
    auto extractor = receive_multipart_part(request, std::nullopt);
    if (extractor == nullptr) {
        return ESP_OK;
    }
    DirectHttpWebVolumePort port(request);
    firmware::application::DirectWebVolumeUpdateService service;
    static_cast<void>(service.apply(extractor->content(), port));
    return ESP_OK;
}

#if CONFIG_HTTPD_WS_SUPPORT
// Receives one video WebSocket frame and applies the shared message boundary.
esp_err_t video_websocket_handler(httpd_req_t* request) {
    // ESP-IDF invokes the URI handler once for the successful HTTP upgrade.
    // Frame bytes are available only on later non-GET callbacks.
    if (request->method == HTTP_GET) {
        const auto socket_id = static_cast<std::uint32_t>(
            httpd_req_to_sockfd(request));
        if (!live_control_policy.on_disconnect(socket_id).empty()) {
            live_generation.fetch_add(1U, std::memory_order_acq_rel);
        }
        return ESP_OK;
    }
    const auto socket_id =
        static_cast<std::uint32_t>(httpd_req_to_sockfd(request));
    httpd_ws_frame_t frame{};
    if (httpd_ws_recv_frame(request, &frame, 0U) != ESP_OK) {
        const auto decisions = live_control_policy.on_disconnect(socket_id);
        if (!decisions.empty()) {
            live_generation.fetch_add(1U, std::memory_order_acq_rel);
        }
        return ESP_FAIL;
    }
    if (frame.len == 0U) return ESP_OK;
    std::vector<std::uint8_t> payload(frame.len);
    frame.payload = payload.data();
    if (httpd_ws_recv_frame(request, &frame, frame.len) != ESP_OK) {
        const auto decisions = live_control_policy.on_disconnect(socket_id);
        if (!decisions.empty()) {
            live_generation.fetch_add(1U, std::memory_order_acq_rel);
        }
        return ESP_FAIL;
    }
    if (frame.type != HTTPD_WS_TYPE_TEXT) {
        return ESP_OK;
    }
    const std::string_view command(
        reinterpret_cast<const char*>(payload.data()), payload.size());
    const auto decisions = live_control_policy.handle(
        socket_id, command);
    for (const auto& decision : decisions) {
        if (decision.action == firmware::application::LiveControlAction::preempted) {
            const std::string message = firmware::core::format_live_preemption("live");
            httpd_ws_frame_t response{};
            response.type = HTTPD_WS_TYPE_TEXT;
            response.payload = reinterpret_cast<uint8_t*>(
                const_cast<char*>(message.data()));
            response.len = message.size();
            static_cast<void>(httpd_ws_send_frame_async(
                request->handle, static_cast<int>(decision.socket_id), &response));
        }
        if (decision.action == firmware::application::LiveControlAction::stop ||
            decision.action == firmware::application::LiveControlAction::preempted) {
            live_generation.fetch_add(1U, std::memory_order_acq_rel);
        }
        if (decision.action == firmware::application::LiveControlAction::start &&
            decision.socket_id == socket_id) {
            if (send_live_frame(request->handle,
                                httpd_req_to_sockfd(request))) {
                start_live_stream(request);
            }
        }
    }
    return ESP_OK;
}

// Receives one preview frame and returns metadata for an accepted open request.
esp_err_t preview_websocket_handler(httpd_req_t* request) {
    // Complete the HTTP upgrade without consuming the first data-frame byte.
    if (request->method == HTTP_GET) {
        return ESP_OK;
    }
    const auto socket_id =
        static_cast<std::uint32_t>(httpd_req_to_sockfd(request));
    httpd_ws_frame_t frame{};
    if (httpd_ws_recv_frame(request, &frame, 0U) != ESP_OK) {
        if (preview_runtime.has_value() &&
            preview_runtime->socket_id == socket_id) {
            preview_generation.fetch_add(1U, std::memory_order_acq_rel);
            preview_runtime.reset();
        }
        return ESP_FAIL;
    }
    if (frame.type != HTTPD_WS_TYPE_TEXT || frame.len == 0U) return ESP_OK;
    std::vector<std::uint8_t> payload(frame.len);
    frame.payload = payload.data();
    if (httpd_ws_recv_frame(request, &frame, frame.len) != ESP_OK) {
        if (preview_runtime.has_value() &&
            preview_runtime->socket_id == socket_id) {
            preview_generation.fetch_add(1U, std::memory_order_acq_rel);
            preview_runtime.reset();
        }
        return ESP_FAIL;
    }
    const auto request_value = firmware::application::accept_preview_socket_message(
        firmware::application::PreviewSocketMessageType::text,
        firmware::core::BytesView(payload));
    if (!request_value.has_value() ||
        request_value->command == firmware::application::PreviewCommand::open) {
        if (!request_value.has_value()) {
            return ESP_OK;
        }
    } else {
        const auto& command = *request_value;
        const bool active = preview_runtime.has_value();
        const bool same_session = active &&
            command.session_id == preview_runtime->session_id &&
            static_cast<std::uint32_t>(httpd_req_to_sockfd(request)) ==
                preview_runtime->socket_id;
        if (!active || !same_session) {
            const auto response = firmware::application::format_preview_conflict(
                command.command, command.sequence,
                active ? preview_runtime->session_id : std::string_view{});
            return send_preview_text(request, response);
        }
        if (command.command == firmware::application::PreviewCommand::play ||
            command.command == firmware::application::PreviewCommand::resume ||
            command.command == firmware::application::PreviewCommand::seek) {
            preview_generation.fetch_add(1U, std::memory_order_acq_rel);
        }
        const auto result = firmware::application::apply_preview_command(
            command, preview_runtime->mode, preview_runtime->current_frame,
            preview_runtime->avi.entries.empty()
                ? 0U
                : static_cast<std::uint32_t>(
                      preview_runtime->avi.entries.size() - 1U),
            preview_runtime->avi.frame_period_us, same_session, active);
        const std::string session_id = preview_runtime->session_id;
        const bool should_send_frame =
            result.reply &&
            (command.command == firmware::application::PreviewCommand::play ||
             command.command == firmware::application::PreviewCommand::seek ||
             command.command == firmware::application::PreviewCommand::resume);
        if (should_send_frame &&
            !send_preview_frame(request, *preview_runtime,
                                 preview_runtime->current_frame)) {
            preview_generation.fetch_add(1U, std::memory_order_acq_rel);
            preview_runtime.reset();
            return ESP_FAIL;
        }
        if (result.reply &&
            (command.command == firmware::application::PreviewCommand::play ||
             command.command == firmware::application::PreviewCommand::resume)) {
            start_preview_playback_task(request, *preview_runtime);
        }
        if (result.terminated) {
            preview_generation.fetch_add(1U, std::memory_order_acq_rel);
            preview_runtime.reset();
        }
        if (result.reply) {
            const auto response = firmware::application::format_preview_response(
                command.command, command.sequence, 0,
                session_id);
            return send_preview_text(request, response);
        }
        return ESP_OK;
    }
    if (!request_value.has_value()) {
        return ESP_OK;
    }
    const auto& preview_request = *request_value;
    const std::string preview_path =
        firmware::core::resolve_sd_user_path(preview_request.path);
    if (!firmware::core::preview_path_allowed(preview_path)) {
        const auto response = firmware::application::format_preview_response(
            preview_request.command, preview_request.sequence, -1);
        return send_preview_text(request, response);
    }
    const auto file = read_preview_file(preview_path);
    if (!file.has_value()) {
        const auto response = firmware::application::format_preview_response(
            preview_request.command, preview_request.sequence, -1);
        return send_preview_text(request, response);
    }
    const auto avi = firmware::core::AviPreview::parse(*file);
    const auto decision = firmware::application::decide_preview_open(
        preview_path, avi.has_value() ? &*avi : nullptr, true, true,
        static_cast<std::uint32_t>(httpd_req_to_sockfd(request)),
        preview_request.sequence);
    if (decision.error != firmware::application::PreviewOpenError::none) {
        const auto response = firmware::application::format_preview_response(
            preview_request.command, preview_request.sequence, -1);
        return send_preview_text(request, response);
    }
    if (preview_runtime.has_value() &&
        preview_runtime->socket_id != socket_id) {
        const std::string message = firmware::core::format_preview_preemption(
            "preview", preview_runtime->session_id);
        httpd_ws_frame_t response{};
        response.type = HTTPD_WS_TYPE_TEXT;
        response.payload = reinterpret_cast<uint8_t*>(
            const_cast<char*>(message.data()));
        response.len = message.size();
        static_cast<void>(httpd_ws_send_frame_async(
            request->handle, static_cast<int>(preview_runtime->socket_id),
            &response));
        preview_generation.fetch_add(1U, std::memory_order_acq_rel);
    }
    preview_runtime = PreviewRuntime{
        *file,
        *avi,
        preview_request.path,
        decision.session_id,
        static_cast<std::uint32_t>(httpd_req_to_sockfd(request)),
        firmware::application::PreviewMode::stopped,
        0U,
    };
    preview_generation.fetch_add(1U, std::memory_order_acq_rel);
    const auto metadata = firmware::application::format_preview_metadata(
        *avi, decision.session_id, preview_request.path,
        preview_request.sequence);
    return send_preview_text(request, metadata);
}
#endif

// Registers the main API, update, and wildcard static-file handlers on port 80.
void register_main_handlers(httpd_handle_t handle) {
    static const httpd_uri_t firmware_info_uri{
        .uri = "/api/firmware/info",
        .method = HTTP_GET,
        .handler = firmware_info_handler,
        .user_ctx = nullptr,
    };
    static const httpd_uri_t camera_resolution_uri{
        .uri = "/api/camera/resolution",
        .method = HTTP_POST,
        .handler = camera_resolution_handler,
        .user_ctx = nullptr,
    };
    static const httpd_uri_t wifi_diagnostics_uri{
        .uri = "/api/wifi/diagnostics",
        .method = HTTP_GET,
        .handler = wifi_diagnostics_handler,
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
    static const httpd_uri_t web_volume_update_uri{
        .uri = "/updateffs",
        .method = HTTP_POST,
        .handler = web_volume_update_handler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(handle, &firmware_info_uri);
    httpd_register_uri_handler(handle, &wifi_diagnostics_uri);
    httpd_register_uri_handler(handle, &camera_resolution_uri);
    httpd_register_uri_handler(handle, &static_file_uri);
    httpd_register_uri_handler(handle, &firmware_update_uri);
    httpd_register_uri_handler(handle, &web_volume_update_uri);
}

#if CONFIG_HTTPD_WS_SUPPORT
// Registers video and preview WebSockets only on the dedicated video server.
void register_video_handlers(httpd_handle_t handle) {
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
    httpd_register_uri_handler(handle, &video_websocket_uri);
    httpd_register_uri_handler(handle, &preview_websocket_uri);
}
#endif

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
    } else {
#if CONFIG_HTTPD_WS_SUPPORT
        register_video_handlers(video_handle_);
#endif
    }
}

}  // namespace firmware::target
