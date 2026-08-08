// Implements TinyUSB vendor callbacks over the transport-neutral USB policies.
#include "usb_device_adapter.hpp"

#include "tinyusb.h"
#include "tusb.h"
#include "class/vendor/vendor_device.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"

#include "firmware/application/usb_descriptors.hpp"
#include "firmware/application/usb_protocol_state.hpp"
#include "firmware/application/usb_transmit_drain.hpp"
#include "firmware/application/recording_commands.hpp"
#include "mock_sd_card_adapter.hpp"
#include "mock_nvs_fault_adapter.hpp"
#include "mock_network_fault_adapter.hpp"
#include "firmware/application/serial_number.hpp"
#include "firmware/application/runtime_commands.hpp"
#include "firmware/application/local_command_queue.hpp"
#include "firmware/application/filesystem_commands.hpp"
#include "firmware/application/directory_listing.hpp"
#include "firmware/application/file_hash_command.hpp"
#include "firmware/application/configuration_get.hpp"
#include "firmware/application/configuration_set.hpp"
#include "firmware/application/configuration_files.hpp"
#include "firmware/application/usb_task_timing.hpp"
#include "firmware/application/wlan_command.hpp"
#include "firmware/application/wlan_request.hpp"
#include "nvs_key_value_adapter.hpp"
#include "wifi_persistence_constants.hpp"
#include "runtime_operation_capacity.hpp"
#include "nvs_command_ports.hpp"
#include "recording_request_state.hpp"
#include "firmware/core/text.hpp"
#include "firmware/core/frame.hpp"
#include "controller_command_loop.hpp"
#include "canopen_target_service.hpp"
#include "tcp_control_adapter.hpp"
#include "tcp_discovery_adapter.hpp"
#include "firmware_update_adapter.hpp"
#include "firmware/application/controller_snapshots.hpp"
#include "runtime_status_adapter.hpp"
#include "firmware/application/runtime_status.hpp"
#include "firmware/application/play_session.hpp"
#include "firmware/application/router.hpp"
#include "play_runtime_state.hpp"
#include "firmware/core/file_transfer_paths.hpp"
#include "firmware/core/sd_user_path.hpp"
#include "firmware/application/file_upload.hpp"
#include "firmware/application/file_download.hpp"
#include "firmware/application/m942_exercise.hpp"
#include "configuration_file_store.hpp"
#include "wlan_event_adapter.hpp"
#include "wifi_diagnostic_log.hpp"
#include "sd_access_diagnostics.hpp"
#include "posix_file.hpp"
#include "esp_wifi_scanner.hpp"

#include <array>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <optional>
#include <memory>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <deque>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <climits>
#include "esp_heap_caps.h"
#include <esp_timer.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace firmware::target {
namespace {

constexpr char tag[] = "usb";
constexpr std::size_t usb_vendor_read_buffer_size = 512U;
constexpr std::uint8_t primary_vendor_interface_index = 0U;
constexpr std::int64_t microseconds_per_millisecond = 1000LL;
constexpr std::uint32_t cache_directory_mode = 0777U;
constexpr std::size_t maximum_cached_md5_text_size = 63U;
constexpr char binary_read_mode[] = "rb";
constexpr char binary_truncate_write_mode[] = "wb";
constexpr std::string_view current_directory_entry = ".";
constexpr std::string_view parent_directory_entry = "..";
constexpr char directory_separator = '/';
constexpr std::uint32_t logged_wifi_delay_threshold_milliseconds = 1000U;
constexpr std::uint32_t local_command_settle_milliseconds = 10U;
constexpr std::uint32_t file_transfer_poll_milliseconds = 50U;
constexpr UBaseType_t usb_worker_priority = 4U;
constexpr std::uint32_t usb_transmit_task_stack_size = 4096U;
constexpr std::uint32_t usb_m942_task_stack_size = 6144U;
constexpr std::uint32_t usb_blocking_worker_stack_size = 8192U;
constexpr std::array<std::uint8_t, 18> device_descriptor{
    0x12U, 0x01U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0x40U,
    0x3aU, 0x30U, 0x02U, 0x40U, 0x00U, 0x01U, 0x01U, 0x02U,
    0x03U, 0x01U};
constexpr std::array<std::uint8_t, 32> configuration_descriptor{
    0x09U, 0x02U, 0x20U, 0x00U, 0x01U, 0x01U, 0x00U, 0x80U,
    0xfaU, 0x09U, 0x04U, 0x00U, 0x00U, 0x02U, 0xffU, 0x00U,
    0x00U, 0x00U, 0x07U, 0x05U, 0x01U, 0x02U, 0x40U, 0x00U,
    0x00U, 0x07U, 0x05U, 0x81U, 0x02U, 0x40U, 0x00U, 0x00U};
// TinyUSB reserves table index zero for the supported USB language ID. Omitting
// it shifts every referenced descriptor and leaves serial index three invalid.
char usb_language_descriptor[] = {0x09, 0x04};
const char* string_descriptors[] = {
    usb_language_descriptor, "Espressif", "MakeraZ1 (USB)", "123456"};
// Derive the TinyUSB count from the table so additions cannot leave a stale
// independent descriptor-count literal behind.
constexpr std::size_t string_descriptor_count =
    sizeof(string_descriptors) / sizeof(string_descriptors[0]);
firmware::application::UsbProtocolState protocol_state;

/// Routes application response frames into the native USB transmit queue.
class UsbFrameSink final : public FrameSink {
public:
    bool send_frame(firmware::core::Frame frame) override {
        const auto encoded = firmware::core::encode_frame(frame);
        return !encoded.empty() && protocol_state.transmit_queue().enqueue(encoded);
    }
};

UsbFrameSink usb_frame_sink;
firmware::core::StreamDecoder decoder(firmware::core::StreamPolicy::usb());
RecordingRequestState recording_state;
firmware::application::LiveConfiguration usb_live_configuration;

/// Provides POSIX-backed configuration I/O while routing replies to USB.
class UsbConfigurationPort final
    : public firmware::application::ConfigurationGetPort,
      public firmware::application::ConfigurationSetPort {
public:
    std::optional<std::vector<firmware::core::ByteVector>>
    read_configuration_chunks(std::size_t maximum_chunk_size) override {
        if (maximum_chunk_size == 0U) return std::nullopt;
        const auto lines = ConfigurationFileStore{}.read_lines();
        std::string content;
        for (const auto& line : lines) {
            content += line;
            content.push_back('\n');
        }
        std::vector<firmware::core::ByteVector> chunks;
        for (std::size_t offset = 0U; offset < content.size();
             offset += maximum_chunk_size) {
            const std::size_t count =
                std::min(maximum_chunk_size, content.size() - offset);
            chunks.emplace_back(content.begin() + offset,
                                content.begin() + offset + count);
        }
        return chunks;
    }

    std::optional<std::string> read_value(std::string_view tag,
                                          std::string_view key) override {
        return ConfigurationFileStore{}.get(tag, key);
    }

    bool set_value(std::string_view tag, std::string_view key,
                   std::string_view value) override {
        return ConfigurationFileStore{}.set(tag, key, value);
    }

    void send(firmware::core::Frame frame) override {
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }
};

UsbConfigurationPort configuration_port;

/// Provides bytewise POSIX copy operations for config restore/default commands.
class UsbConfigurationFilePort final
    : public firmware::application::ConfigurationFilePort {
public:
    std::string_view active_configuration_path() const override {
        return firmware::target::active_configuration_path();
    }
    std::string_view default_configuration_path() const override {
        static const std::string path =
            firmware::core::physical_sd_path("/config.default");
        return path;
    }
    ~UsbConfigurationFilePort() override {
        close_source();
        close_destination();
    }

    bool file_exists(std::string_view path) override {
        struct stat information{};
        const int result = stat(std::string(path).c_str(), &information);
        if (result == 0 && S_ISREG(information.st_mode)) return true;
        const int error_number = result == 0 ? EINVAL : errno;
        firmware::target::log_sd_access_failure("inspect download file", path,
                                                error_number);
        return false;
    }

    bool open_source(std::string_view path) override {
        close_source();
        source_ = std::fopen(std::string(path).c_str(), binary_read_mode);
        return source_ != nullptr;
    }

    bool open_truncated_destination(std::string_view path) override {
        close_destination();
        destination_ =
            std::fopen(std::string(path).c_str(), binary_truncate_write_mode);
        return destination_ != nullptr;
    }

    firmware::application::ByteRead read_byte() override {
        if (source_ == nullptr) {
            return {firmware::application::ByteReadStatus::failure, 0U};
        }
        std::uint8_t value = 0U;
        if (std::fread(&value, 1U, 1U, source_) == 1U) {
            return {firmware::application::ByteReadStatus::byte, value};
        }
        return {std::ferror(source_) != 0
                    ? firmware::application::ByteReadStatus::failure
                    : firmware::application::ByteReadStatus::end_of_file,
                0U};
    }

    bool write_byte(std::uint8_t value) override {
        return destination_ != nullptr &&
               std::fwrite(&value, 1U, 1U, destination_) == 1U;
    }

    void close_source() override {
        if (source_ != nullptr) std::fclose(source_);
        source_ = nullptr;
    }

    bool close_destination() override {
        if (destination_ == nullptr) return true;
        const bool flushed = std::fflush(destination_) == 0;
        const bool closed = std::fclose(destination_) == 0;
        destination_ = nullptr;
        return flushed && closed;
    }

    void send(firmware::core::Frame frame) override {
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }

private:
    FILE* source_ = nullptr;
    FILE* destination_ = nullptr;
};

UsbConfigurationFilePort configuration_file_port;

/// Performs blocking ESP-IDF scans and queues WLAN responses on USB.
class UsbWlanScanPort final : public firmware::application::WlanCommandPort {
public:
    void stop_scan() override {
        EspWifiScanner{}.stop_scan();
    }

    void delay_milliseconds(std::uint32_t duration) override {
        vTaskDelay(pdMS_TO_TICKS(duration));
    }

    firmware::application::WifiScanOutcome scan(
        const firmware::application::WifiScanConfig& config) override {
        return EspWifiScanner{}.scan(config);
    }

    std::string connected_ssid() const override {
        return EspWifiScanner{}.connected_ssid();
    }

    void send(firmware::core::Frame frame) override {
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }
};

UsbWlanScanPort wlan_scan_port;

/// Converts an ESP-IDF result into the transport-neutral station result.
///
/// @param result ESP-IDF result returned by the station operation.
/// @param operation Stable operation name included in a failure result.
/// @return A successful result for `ESP_OK`, otherwise a failed result carrying
///         the supplied operation name.
firmware::application::StationApiResult api_result(esp_err_t result,
                                                    const char* operation) {
    return result == ESP_OK
               ? firmware::application::StationApiResult{true, {}}
               : firmware::application::StationApiResult{false, operation};
}

/// Implements manual station operations using ESP-IDF and shared NVS storage.
class UsbWlanStationPort final
    : public firmware::application::StationConnectionPort {
public:
    firmware::application::StationApiResult request_disconnect() override {
        return api_result(esp_wifi_disconnect(), "disconnect");
    }

    firmware::application::StationApiResult apply_station_config(
        const firmware::application::StationConfiguration& configuration) override {
        wifi_config_t wifi_config{};
        const std::size_t ssid_size =
            std::min(configuration.ssid.size(), sizeof(wifi_config.sta.ssid));
        const std::size_t password_size =
            std::min(configuration.password.size(), sizeof(wifi_config.sta.password));
        std::memcpy(wifi_config.sta.ssid, configuration.ssid.data(), ssid_size);
        std::memcpy(wifi_config.sta.password, configuration.password.data(),
                    password_size);
        static_cast<void>(firmware::target::wifi_diagnostic_log().trace(
            "wifi.config.request ssid_length=" + std::to_string(ssid_size) +
            " password_length=" + std::to_string(password_size)));
        wifi_config.sta.scan_method = WIFI_FAST_SCAN;
        wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        wifi_config.sta.threshold.authmode = static_cast<wifi_auth_mode_t>(
            configuration.minimum_authentication_mode);
        // Allow WPA2 access points that require protected management frames
        // while keeping PMF optional for ordinary WPA2-PSK networks.
        wifi_config.sta.pmf_cfg.capable = true;
        wifi_config.sta.pmf_cfg.required = false;
        // Keep the provisioning access point alive while the station joins;
        // changing to station-only mode disrupts the native USB transport.
        const esp_err_t mode_result = esp_wifi_set_mode(WIFI_MODE_APSTA);
        static_cast<void>(firmware::target::wifi_diagnostic_log().trace(
            mode_result == ESP_OK ? "wifi.set_mode.ok" : "wifi.set_mode.error"));
        if (mode_result != ESP_OK) return api_result(mode_result, "set_mode");
        const esp_err_t config_result =
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        static_cast<void>(firmware::target::wifi_diagnostic_log().trace(
            config_result == ESP_OK ? "wifi.set_config.ok"
                                    : "wifi.set_config.error"));
        return api_result(config_result, "set_config");
    }

    firmware::application::StationApiResult request_connect() override {
        const esp_err_t result = esp_wifi_connect();
        static_cast<void>(firmware::target::wifi_diagnostic_log().trace(
            result == ESP_OK ? "wifi.connect.requested"
                             : "wifi.connect.error"));
        return api_result(result, "connect");
    }

    void delay_milliseconds(std::uint32_t duration) override {
        if (duration >= logged_wifi_delay_threshold_milliseconds) {
            static_cast<void>(firmware::target::wifi_diagnostic_log().trace(
                "manual.delay ms=" + std::to_string(duration)));
        }
        vTaskDelay(pdMS_TO_TICKS(duration));
    }

    firmware::application::StationSnapshot station_snapshot() const override {
        wifi_ap_record_t access_point{};
        if (esp_wifi_sta_get_ap_info(&access_point) != ESP_OK) {
            log_state(firmware::application::StationConnectionState::attempt_started);
            return {};
        }
        const std::string ssid(reinterpret_cast<const char*>(access_point.ssid));
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif == nullptr) {
            log_state(firmware::application::StationConnectionState::associated);
            return {firmware::application::StationConnectionState::associated,
                    ssid, {}};
        }
        esp_netif_ip_info_t ip_info{};
        if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK ||
            ip_info.ip.addr == 0U) {
            log_state(firmware::application::StationConnectionState::associated);
            return {firmware::application::StationConnectionState::associated,
                    ssid, {}};
        }
        char address[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &ip_info.ip.addr, address, sizeof(address));
        log_state(firmware::application::StationConnectionState::address_ready);
        return {firmware::application::StationConnectionState::address_ready,
                ssid, address};
    }

    std::string connection_error_detail() const override {
        const std::string detail = firmware::target::last_station_disconnect_detail();
        static_cast<void>(firmware::target::wifi_diagnostic_log().append(
            "wifi.connection.detail=" + detail));
        return detail;
    }

    void record_diagnostic(std::string_view message) override {
        static_cast<void>(firmware::target::wifi_diagnostic_log().trace(message));
    }

    firmware::application::StationApiResult save_credentials(
        std::string_view ssid, std::string_view password) override {
        NvsKeyValueAdapter nvs;
        if (!nvs.write_string(wifi_persistence::name_space,
                              wifi_persistence::ssid_key, ssid)) {
            static_cast<void>(firmware::target::wifi_diagnostic_log().append(
                "wifi.credentials.save_ssid.error"));
            return {false, "save_ssid"};
        }
        if (!nvs.write_string(wifi_persistence::name_space,
                              wifi_persistence::password_key, password)) {
            static_cast<void>(firmware::target::wifi_diagnostic_log().append(
                "wifi.credentials.save_password.error"));
            return {false, "save_password"};
        }
        static_cast<void>(firmware::target::wifi_diagnostic_log().append(
            "wifi.credentials.saved"));
        return {true, {}};
    }

private:
    // Records only state transitions so polling does not flood the persistent log.
    void log_state(firmware::application::StationConnectionState state) const {
        if (state == last_logged_state_) return;
        last_logged_state_ = state;
        const char* name = "unknown";
        if (state == firmware::application::StationConnectionState::attempt_started) {
            name = "attempt_started";
        } else if (state == firmware::application::StationConnectionState::associated) {
            name = "associated";
        } else if (state == firmware::application::StationConnectionState::address_ready) {
            name = "address_ready";
        }
        static_cast<void>(firmware::target::wifi_diagnostic_log().append(
            std::string("wifi.snapshot.state=") + name));
    }

    mutable firmware::application::StationConnectionState last_logged_state_{
        firmware::application::StationConnectionState::idle};
};

// Routes WLAN connection responses to the USB transmit queue.
/// Sends WLAN connection and discovery responses through native USB.
class UsbWlanResponsePort final
    : public firmware::application::WlanConnectionResponsePort {
public:
    void send(firmware::core::Frame frame) override {
        static_cast<void>(firmware::target::wifi_diagnostic_log().append(
            "wlan.response type=" + std::to_string(frame.type)));
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }

    void delay_milliseconds(std::uint32_t duration) override {
        vTaskDelay(pdMS_TO_TICKS(duration));
    }

    void send_discovery_burst() override {
        send_tcp_discovery_burst(active_tcp_client_count());
    }
};

UsbWlanStationPort wlan_station_port;
UsbWlanResponsePort wlan_response_port;
firmware::application::StationRuntime usb_station_runtime;

// Provides POSIX file preparation and origin-aware responses for USB play.
/// Prepares a requested G-code file and broadcasts playback responses.
class UsbPlayPreparationPort final
    : public firmware::application::PlayPreparationPort {
public:
    ~UsbPlayPreparationPort() override {
        close_file();
    }

    void close_file() override {
        if (file_ != nullptr) std::fclose(file_);
        file_ = nullptr;
    }

    std::optional<std::uint64_t> open_file(std::string_view path) override {
        close_file();
        file_ = std::fopen(std::string(path).c_str(), binary_read_mode);
        if (file_ == nullptr) {
            firmware::target::log_sd_access_failure("open play file", path, errno);
            return std::nullopt;
        }
        if (std::fseek(file_, 0L, SEEK_END) != 0) {
            firmware::target::log_sd_access_failure("seek play file", path, errno);
            close_file();
            return std::nullopt;
        }
        const long size = std::ftell(file_);
        if (size < 0L || std::fseek(file_, 0L, SEEK_SET) != 0) {
            close_file();
            return std::nullopt;
        }
        return static_cast<std::uint64_t>(size);
    }

    std::optional<std::string> cached_md5(std::string_view path) override {
        const auto cache = firmware::core::map_file_cache_paths(path).md5_path;
        if (!cache.has_value()) return std::nullopt;
        const auto bytes =
            read_posix_file(*cache, maximum_cached_md5_text_size);
        if (!bytes.has_value()) return std::nullopt;
        return firmware::core::extract_cached_md5(*bytes);
    }

    void broadcast(firmware::core::Frame frame) override {
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
            firmware::target::broadcast_tcp_frame(frame);
        }
    }

private:
    FILE* file_ = nullptr;
};

UsbPlayPreparationPort usb_play_port;

// Implements upload filesystem effects and USB-origin response delivery.
/// Provides the USB upload state machine with POSIX-backed file operations.
class UsbFileUploadPort final : public firmware::application::FileUploadPort {
public:
    ~UsbFileUploadPort() override { close_files(); }

    void prepare_cache_paths(const firmware::core::FileCachePaths& paths) override {
        if (paths.md5_path.has_value()) {
            create_parent_directories(*paths.md5_path, cache_directory_mode);
        }
        if (paths.compressed_path.has_value()) {
            create_parent_directories(*paths.compressed_path,
                                      cache_directory_mode);
        }
    }

    bool create_parent_directories(std::string_view path,
                                   std::uint32_t mode) override {
        std::string value(path);
        const std::size_t slash = value.find_last_of('/');
        if (slash == std::string::npos) return true;
        value.resize(slash);
        for (std::size_t position = 1U; position <= value.size(); ++position) {
            if (position != value.size() && value[position] != '/') continue;
            const std::string part = value.substr(0U, position);
            if (mkdir(part.c_str(), static_cast<mode_t>(mode)) != 0 &&
                errno != EEXIST) return false;
        }
        return true;
    }

    bool open_primary(std::string_view path) override {
        close_files();
        primary_path_ = path;
        primary_ =
            std::fopen(std::string(path).c_str(), binary_truncate_write_mode);
        if (primary_ == nullptr) {
            firmware::target::log_sd_access_failure("open upload file", path, errno);
        }
        return primary_ != nullptr;
    }

    bool open_md5(std::string_view path) override {
        md5_path_ = path;
        md5_ =
            std::fopen(std::string(path).c_str(), binary_truncate_write_mode);
        if (md5_ == nullptr) {
            firmware::target::log_sd_access_failure("open MD5 file", path, errno);
        }
        return md5_ != nullptr;
    }

    bool write_primary(firmware::core::BytesView data) override {
        return write_all(primary_, primary_path_, data);
    }

    bool write_md5(firmware::core::BytesView data) override {
        return write_all(md5_, md5_path_, data);
    }

    void close_files() override {
        if (primary_ != nullptr) std::fclose(primary_);
        if (md5_ != nullptr) std::fclose(md5_);
        primary_ = nullptr;
        md5_ = nullptr;
        primary_path_.clear();
        md5_path_.clear();
    }

    void flush_and_close() override {
        if (primary_ != nullptr) {
            std::fflush(primary_);
            fsync(fileno(primary_));
        }
        if (md5_ != nullptr) {
            std::fflush(md5_);
            fsync(fileno(md5_));
        }
        close_files();
    }

    bool remove_file(std::string_view path) override {
        return unlink(std::string(path).c_str()) == 0;
    }

    bool rename_file(std::string_view source,
                     std::string_view destination) override {
        if (rename(std::string(source).c_str(),
                   std::string(destination).c_str()) == 0) return true;
        firmware::target::log_sd_access_failure("rename upload file", source, errno);
        return false;
    }

    void send(const firmware::application::HostIdentity&,
              firmware::core::Frame frame) override {
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }

    void release_ownership() override {}

private:
    static bool write_all(FILE* file, std::string_view path,
                          firmware::core::BytesView data) {
        if (file == nullptr) return false;
        std::size_t written = 0U;
        while (written < data.size()) {
            const std::size_t count =
                std::fwrite(data.data() + written, 1U, data.size() - written,
                            file);
            if (count == 0U) {
                firmware::target::log_sd_access_failure("write upload file", path,
                                                        errno);
                return false;
            }
            written += count;
        }
        return true;
    }

    FILE* primary_ = nullptr;
    FILE* md5_ = nullptr;
    std::string primary_path_;
    std::string md5_path_;
};

UsbFileUploadPort usb_upload_port;
firmware::application::FileUpload usb_upload;
const firmware::application::HostIdentity usb_host_identity{
    firmware::application::HostTransport::usb, 0U, 0U};

// Implements bounded POSIX reads and MD5 lookup for USB downloads.
/// Provides the USB download state machine with POSIX-backed file operations.
class UsbFileDownloadPort final : public firmware::application::FileDownloadPort {
public:
    ~UsbFileDownloadPort() override { close_file(); }

    void prepare_cache_paths(const firmware::core::FileCachePaths& paths) override {
        if (paths.md5_path.has_value()) usb_upload_port.create_parent_directories(
            *paths.md5_path, cache_directory_mode);
        if (paths.compressed_path.has_value()) usb_upload_port.create_parent_directories(
            *paths.compressed_path, cache_directory_mode);
    }

    std::optional<std::string> calculate_md5(std::string_view path) override {
        return calculate_posix_md5(path);
    }

    std::optional<firmware::core::ByteVector> read_cache(
        std::string_view path, std::size_t maximum_size) override {
        FILE* input = std::fopen(std::string(path).c_str(), binary_read_mode);
        if (input == nullptr) {
            firmware::target::log_sd_access_failure("open cache file", path, errno);
            return std::nullopt;
        }
        firmware::core::ByteVector data(maximum_size);
        const std::size_t count = std::fread(data.data(), 1U, maximum_size, input);
        data.resize(count);
        std::fclose(input);
        return data;
    }

    bool file_exists(std::string_view path) override {
        struct stat information{};
        return stat(std::string(path).c_str(), &information) == 0 &&
               S_ISREG(information.st_mode);
    }

    std::optional<std::uint64_t> open_file(std::string_view path) override {
        if (!file_.open(path, binary_read_mode)) return std::nullopt;
        return file_.size();
    }

    std::optional<firmware::core::ByteVector> read_file(
        std::uint64_t offset, std::size_t maximum_size) override {
        return file_.read_at(offset, maximum_size);
    }

    bool allocate_response_workspace(std::size_t size) override {
        void* workspace = heap_caps_malloc(size, MALLOC_CAP_8BIT);
        if (workspace == nullptr) return false;
        heap_caps_free(workspace);
        return true;
    }

    void close_file() override {
        file_.close();
    }

    void send(const firmware::application::HostIdentity&,
              firmware::core::Frame frame) override {
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }

    void release_ownership() override { close_file(); }

private:
    firmware::target::PosixFile file_;
};

UsbFileDownloadPort usb_download_port;
firmware::application::FileDownload usb_download;

// Adapts M942 CAN exercise responses and remote SDO access to USB.
/// Bridges an M942 exercise request between native USB and the controller.
class UsbM942Port final : public firmware::application::M942ExercisePort {
public:
    void forward_to_controller(const firmware::core::Frame& frame) override {
        static_cast<void>(enqueue_controller_frame(frame));
    }

    void respond(const firmware::application::HostIdentity& host,
                 const firmware::core::Frame& frame) override {
        if (!(host == usb_host_identity)) return;
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }

    std::uint64_t monotonic_milliseconds() const override {
        return static_cast<std::uint64_t>(
            esp_timer_get_time() / microseconds_per_millisecond);
    }

    void delay_milliseconds(std::uint32_t duration) override {
        vTaskDelay(pdMS_TO_TICKS(duration));
    }

    void lock_sdo_client() override {}
    void unlock_sdo_client() override {}

    std::optional<std::uint32_t> read_remote_u32(
        std::uint8_t node, std::uint16_t index, std::uint8_t subindex,
        std::uint32_t timeout, std::uint64_t) override {
        auto* service = active_canopen_target_service();
        return service == nullptr
                   ? std::nullopt
                   : service->read_remote_u32(node, index, subindex, timeout);
    }

    bool write_remote_u32(std::uint8_t node, std::uint16_t index,
                          std::uint8_t subindex, std::uint32_t value,
                          std::uint32_t timeout, std::uint64_t) override {
        auto* service = active_canopen_target_service();
        return service != nullptr && service->write_remote_u32(
            node, index, subindex, value, timeout);
    }
};

UsbM942Port usb_m942_port;

// Owns one asynchronous USB M942 execution until its service terminates.
/// Runs one asynchronous M942 exercise and then deletes the current task.
///
/// @param parameter Owned heap-allocated M942 request data.
void usb_m942_task(void* parameter) {
    auto* service = static_cast<firmware::application::M942ExerciseService*>(
        parameter);
    service->run();
    release_m942_worker();
    delete service;
    vTaskDelete(nullptr);
}
SemaphoreHandle_t usb_file_mutex = nullptr;
constexpr std::size_t maximum_pending_usb_file_frames = 8U;
std::deque<firmware::core::Frame> pending_usb_file_frames;

NvsSerialNumberPort serial_port(usb_frame_sink);
NvsRuntimeCommandPort runtime_port(usb_frame_sink);

/// Removes a file or directory tree used by a USB filesystem command.
///
/// @param path Physical sandboxed path to remove recursively.
void remove_usb_tree(const std::string& path) {
    struct stat status{};
    if (stat(path.c_str(), &status) != 0) return;
    if (!S_ISDIR(status.st_mode)) {
        static_cast<void>(unlink(path.c_str()));
        return;
    }
    DIR* directory = opendir(path.c_str());
    if (directory != nullptr) {
        while (const dirent* entry = readdir(directory)) {
            const std::string name(entry->d_name);
            if (name == current_directory_entry ||
                name == parent_directory_entry) {
                continue;
            }
            remove_usb_tree(path + directory_separator + name);
        }
        closedir(directory);
    }
    static_cast<void>(rmdir(path.c_str()));
}

/// Implements directory creation, inspection, and removal for USB commands.
class UsbFilesystemPort final
    : public firmware::application::FilesystemCommandPort {
public:
    bool create_directory(std::string_view path, std::uint32_t mode) override {
        const std::string value(path);
        if (mkdir(value.c_str(), static_cast<mode_t>(mode)) == 0 ||
            errno == EEXIST) return true;
        firmware::target::log_sd_access_failure("create directory", path, errno);
        return false;
    }

    void remove_recursively(std::string_view path) override {
        remove_usb_tree(std::string(path));
    }

    bool path_exists(std::string_view path) override {
        struct stat status{};
        const std::string value(path);
        return stat(value.c_str(), &status) == 0;
    }

    bool rename_path(std::string_view source,
                     std::string_view destination) override {
        const std::string old_path(source);
        const std::string new_path(destination);
        if (rename(old_path.c_str(), new_path.c_str()) == 0) return true;
        firmware::target::log_sd_access_failure("rename", source, errno);
        return false;
    }

    void send(firmware::core::Frame frame) override {
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }
};

UsbFilesystemPort filesystem_port;

/// Converts a POSIX timestamp to the protocol's UTC file-time representation.
///
/// @param value POSIX timestamp to convert.
/// @return Calendar fields in UTC, or zero-initialized fields on failure.
firmware::application::UtcFileTime usb_file_time(time_t value) {
    struct tm result{};
    gmtime_r(&value, &result);
    return {static_cast<std::uint16_t>(result.tm_year + 1900),
            static_cast<std::uint8_t>(result.tm_mon + 1),
            static_cast<std::uint8_t>(result.tm_mday),
            static_cast<std::uint8_t>(result.tm_hour),
            static_cast<std::uint8_t>(result.tm_min),
            static_cast<std::uint8_t>(result.tm_sec)};
}

/// Lists sandboxed SD directories and sends their entries through native USB.
class UsbDirectoryPort final : public firmware::application::DirectoryListPort {
public:
    std::optional<std::vector<firmware::application::DirectoryEntry>>
    list_directory(std::string_view path) override {
        const std::string root(path);
        if (!firmware::target::sd_storage_mounted()) {
            firmware::target::log_sd_access_failure(
                "open directory", root, ENODEV);
            return std::nullopt;
        }
        DIR* directory = opendir(root.c_str());
        if (directory == nullptr) {
            const int error_number = errno;
            ESP_LOGE("APP_FILE", "Can't open dir: %s", root.c_str());
            firmware::target::log_sd_access_failure(
                "open directory", root, error_number);
            return std::nullopt;
        }
        std::vector<firmware::application::DirectoryEntry> entries;
        while (const dirent* item = readdir(directory)) {
            const std::string name(item->d_name);
            if (name == current_directory_entry ||
                name == parent_directory_entry) {
                continue;
            }
            struct stat information{};
            const std::string full_path = root + directory_separator + name;
            const bool metadata = stat(full_path.c_str(), &information) == 0;
            entries.push_back({name, metadata && S_ISDIR(information.st_mode),
                               metadata ? static_cast<std::uint64_t>(information.st_size) : 0U,
                               metadata ? usb_file_time(information.st_mtime) : usb_file_time(0),
                               metadata});
        }
        closedir(directory);
        return entries;
    }

    void send(firmware::core::Frame frame) override {
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }
};

/// Calculates file hashes and sends hash responses through native USB.
class UsbHashPort final : public firmware::application::FileHashPort {
public:
    firmware::application::FileHashPathState inspect_path(
        std::string_view path) override {
        struct stat information{};
        if (stat(std::string(path).c_str(), &information) != 0) {
            firmware::target::log_sd_access_failure("inspect path", path, errno);
            return firmware::application::FileHashPathState::missing;
        }
        return S_ISREG(information.st_mode)
                   ? firmware::application::FileHashPathState::regular_file
                   : firmware::application::FileHashPathState::not_regular;
    }

    std::optional<std::string> calculate_md5(std::string_view path,
                                             std::size_t block_size) override {
        return calculate_posix_md5(path, block_size);
    }

    void send(firmware::core::Frame frame) override {
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }
};

UsbDirectoryPort directory_port;
UsbHashPort hash_port;
firmware::application::LocalCommandQueue usb_local_commands;
SemaphoreHandle_t usb_local_command_mutex = nullptr;

/// Forwards USB bytes from TinyUSB into the transport-neutral frame decoder.
///
/// @param bytes Contiguous bytes read from the vendor endpoint.
/// @param size Number of readable bytes at @p bytes.
void consume_received_bytes(const std::uint8_t* bytes, std::size_t size);

/// Enqueues one USB-local frame while serializing callback/task access.
///
/// @param frame Complete decoded command frame to copy into the queue.
/// @return `true` when the frame was queued; otherwise `false`.
bool enqueue_usb_local_command(const firmware::core::Frame& frame) {
    if (usb_local_command_mutex == nullptr ||
        xSemaphoreTake(usb_local_command_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    const bool queued = usb_local_commands.enqueue(frame);
    xSemaphoreGive(usb_local_command_mutex);
    return queued;
}

/// Removes one USB-local frame while serializing callback/task access.
///
/// @return The oldest pending frame, or `std::nullopt` when none is available.
std::optional<firmware::core::Frame> dequeue_usb_local_command() {
    if (usb_local_command_mutex == nullptr ||
        xSemaphoreTake(usb_local_command_mutex, portMAX_DELAY) != pdTRUE) {
        return std::nullopt;
    }
    auto frame = usb_local_commands.dequeue();
    xSemaphoreGive(usb_local_command_mutex);
    return frame;
}

/// Processes non-file commands outside the TinyUSB receive callback.
///
/// Some commands perform blocking filesystem, NVS, or controller work. Running
/// them in TinyUSB's callback previously starved USB servicing and could make
/// the host time out, so the callback only enqueues them for this worker.
///
/// @param unused FreeRTOS task parameter; shared state is module-owned.
void usb_local_command_task(void* /* unused */) {
    for (;;) {
        const auto command_frame = dequeue_usb_local_command();
        if (!command_frame.has_value()) {
            vTaskDelay(pdMS_TO_TICKS(
                firmware::application::usb_task_poll_delay_milliseconds));
            continue;
        }
        const auto match = firmware::core::recognize_command(
            command_frame->payload);
        const std::string_view command(
            reinterpret_cast<const char*>(command_frame->payload.data()),
            command_frame->payload.size());
        if (match.kind == firmware::core::CommandKind::record_start ||
            match.kind == firmware::core::CommandKind::record_stop) {
            const auto result = firmware::application::handle_recording_command(
                match.kind, recording_state.requested());
            recording_state.set_requested(result.requested);
            const auto encoded = firmware::core::encode_frame(result.response);
            if (!encoded.empty()) {
                static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
            }
        } else if (match.kind == firmware::core::CommandKind::serial_get ||
                   match.kind == firmware::core::CommandKind::serial_set) {
            firmware::application::SerialNumberService service(serial_port);
            if (match.kind == firmware::core::CommandKind::serial_get) {
                service.handle_get(command);
            } else {
                service.handle_set(command);
            }
        } else if (match.kind == firmware::core::CommandKind::system_time ||
                   match.kind == firmware::core::CommandKind::clear_first_time) {
            firmware::application::RuntimeCommandService service(runtime_port);
            if (match.kind == firmware::core::CommandKind::system_time) {
                service.handle_system_time(command);
            } else {
                service.handle_clear_first_boot(command);
            }
        } else if (match.kind == firmware::core::CommandKind::upgrade ||
                   match.kind == firmware::core::CommandKind::reset) {
            request_firmware_update_processing();
        } else if (match.kind == firmware::core::CommandKind::make_directory ||
                   match.kind == firmware::core::CommandKind::remove ||
                   match.kind == firmware::core::CommandKind::move ||
                   match.kind == firmware::core::CommandKind::file_type) {
            const firmware::core::BytesView argument(
                command_frame->payload.data() + match.argument_offset,
                command_frame->payload.size() - match.argument_offset);
            if (match.kind == firmware::core::CommandKind::make_directory) {
                firmware::application::FilesystemCommands::make_directory(
                    argument, filesystem_port);
            } else if (match.kind == firmware::core::CommandKind::remove) {
                firmware::application::FilesystemCommands::remove(
                    argument, filesystem_port);
            } else if (match.kind == firmware::core::CommandKind::move) {
                firmware::application::FilesystemCommands::move(
                    argument, filesystem_port);
            } else {
                firmware::application::FilesystemCommands::file_type(
                    filesystem_port);
            }
        } else if (match.kind == firmware::core::CommandKind::list) {
            const firmware::core::BytesView argument(
                command_frame->payload.data() + match.argument_offset,
                command_frame->payload.size() - match.argument_offset);
            firmware::application::DirectoryListing::execute(
                argument, directory_port);
        } else if (match.kind == firmware::core::CommandKind::md5_sum) {
            const firmware::core::BytesView argument(
                command_frame->payload.data() + match.argument_offset,
                command_frame->payload.size() - match.argument_offset);
            firmware::application::FileHashCommand::execute(
                argument, hash_port);
        } else if (match.kind == firmware::core::CommandKind::wlan) {
            static_cast<void>(firmware::target::wifi_diagnostic_log().append(
                "wlan.command.start"));
            const auto request = firmware::application::parse_wlan_request(command);
            static_cast<void>(firmware::target::wifi_diagnostic_log().append(
                request.kind == firmware::application::WlanRequestKind::connect
                    ? "wlan.request.connect"
                    : request.kind == firmware::application::WlanRequestKind::save
                          ? "wlan.request.save"
                    : request.kind == firmware::application::WlanRequestKind::disconnect
                          ? "wlan.request.disconnect"
                          : request.kind == firmware::application::WlanRequestKind::scan
                                ? "wlan.request.scan"
                                : "wlan.request.invalid"));
            if (request.kind == firmware::application::WlanRequestKind::save) {
                const auto result = wlan_station_port.save_credentials(
                    request.ssid, request.password);
                if (result.success) {
                    wlan_response_port.send({
                        firmware::core::protocol::operation_success,
                        {'o', 'k', '\r', '\n'}});
                } else {
                    std::string message = "Error: ";
                    message += result.error_name;
                    message.push_back('\n');
                    wlan_response_port.send({
                        firmware::core::protocol::operation_failure,
                        {message.begin(), message.end()}});
                }
            } else if (request.kind == firmware::application::WlanRequestKind::scan) {
                firmware::application::WlanScanCommand::execute(wlan_scan_port);
            } else if (request.kind ==
                       firmware::application::WlanRequestKind::disconnect) {
                firmware::application::WlanConnectionCommand::disconnect(
                    usb_station_runtime, wlan_station_port, wlan_response_port);
            } else {
                firmware::application::WlanConnectionCommand::connect(
                    usb_station_runtime, wlan_station_port, wlan_response_port,
                    request.ssid, request.password);
            }
            static_cast<void>(firmware::target::wifi_diagnostic_log().append(
                "wlan.command.finish"));
        } else if (match.kind == firmware::core::CommandKind::config_restore ||
                   match.kind == firmware::core::CommandKind::config_default) {
            if (match.kind == firmware::core::CommandKind::config_restore) {
                firmware::application::ConfigurationFiles::restore(
                    configuration_file_port);
            } else {
                firmware::application::ConfigurationFiles::save_default(
                    configuration_file_port);
            }
        } else if (match.kind == firmware::core::CommandKind::config_get ||
                   match.kind == firmware::core::CommandKind::config_set) {
            const firmware::core::BytesView argument(
                command_frame->payload.data() + match.argument_offset,
                command_frame->payload.size() - match.argument_offset);
            if (match.kind == firmware::core::CommandKind::config_get) {
                firmware::application::ConfigurationGet::execute(
                    argument, usb_live_configuration,
                    configuration_port);
            } else {
                firmware::application::ConfigurationSet::execute(
                    argument, usb_live_configuration,
                    configuration_port);
            }
        } else if (match.kind == firmware::core::CommandKind::diagnose) {
            const auto response = shared_controller_snapshots().diagnostic_reply(0);
            if (response.has_value()) {
                static_cast<void>(protocol_state.transmit_queue().enqueue(
                    firmware::core::encode_frame(*response)));
            }
        } else if (match.kind == firmware::core::CommandKind::wifi_diagnose) {
            const std::string log = firmware::target::wifi_diagnostic_log().read();
            const std::string_view payload = log.empty() ? "<empty>\n" : log;
            constexpr std::size_t diagnostic_chunk_size = 48U;
            for (std::size_t offset = 0U; offset < payload.size();
                 offset += diagnostic_chunk_size) {
                const std::size_t count =
                    std::min(diagnostic_chunk_size, payload.size() - offset);
                static_cast<void>(protocol_state.transmit_queue().enqueue(
                    firmware::core::encode_frame(
                        {firmware::core::protocol::text_response,
                         {payload.begin() + static_cast<std::ptrdiff_t>(offset),
                          payload.begin() + static_cast<std::ptrdiff_t>(offset + count)}})));
            }
        } else if (match.kind == firmware::core::CommandKind::mock_sd_control) {
            const std::string response = handle_mock_sd_control(command);
            static_cast<void>(usb_frame_sink.send_frame(
                {firmware::core::protocol::text_response,
                 {response.begin(), response.end()}}));
        } else if (match.kind == firmware::core::CommandKind::mock_nvs_control) {
            const std::string response = handle_mock_nvs_control(command);
            static_cast<void>(usb_frame_sink.send_frame(
                {firmware::core::protocol::text_response,
                 {response.begin(), response.end()}}));
        } else if (match.kind == firmware::core::CommandKind::mock_network_control) {
            const std::string response = handle_mock_network_control(command);
            static_cast<void>(usb_frame_sink.send_frame(
                {firmware::core::protocol::text_response,
                 {response.begin(), response.end()}}));
        } else if (match.kind == firmware::core::CommandKind::version) {
            const auto response = shared_controller_snapshots().version_reply();
            static_cast<void>(protocol_state.transmit_queue().enqueue(
                firmware::core::encode_frame(response)));
        }
        vTaskDelay(pdMS_TO_TICKS(local_command_settle_milliseconds));
    }
}

/// Adapts the portable transmit drain to TinyUSB's vendor endpoint FIFO.
class TinyUsbTransmitDrainPort final
    : public firmware::application::UsbTransmitDrainPort {
public:
    /// Returns the currently writable vendor-endpoint capacity.
    std::size_t available() override {
        return tud_vendor_write_available();
    }

    /// Submits one fragment without assuming TinyUSB accepts it completely.
    std::size_t write(firmware::core::BytesView bytes) override {
        return tud_vendor_write(bytes.data(),
                                static_cast<std::uint32_t>(bytes.size()));
    }

    /// Forces short final fragments out of TinyUSB's software FIFO.
    void flush() override {
        tud_vendor_flush();
    }
};

/// Drains queued response frames into the TinyUSB vendor endpoint.
///
/// @param unused FreeRTOS task parameter; shared state is module-owned.
void usb_transmit_task(void* /* unused */) {
    TinyUsbTransmitDrainPort endpoint;
    firmware::application::UsbTransmitDrain drain(
        protocol_state.transmit_queue(), endpoint);
    for (;;) {
        drain.process(
            protocol_state.can_send(),
            static_cast<std::uint64_t>(
                esp_timer_get_time() / microseconds_per_millisecond));
        vTaskDelay(pdMS_TO_TICKS(
            firmware::application::usb_task_poll_delay_milliseconds));
    }
}

/// Polls the buffered TinyUSB vendor FIFO and dispatches received bytes.
///
/// This keeps receive behavior consistent across ESP-IDF TinyUSB buffer
/// configurations. In buffered callback mode TinyUSB may invoke the callback
/// without supplying the bytes directly, so this polling worker also drains
/// data that would otherwise remain unread in the vendor FIFO.
///
/// @param unused FreeRTOS task parameter; shared state is module-owned.
void usb_receive_task(void* /* unused */) {
    std::array<std::uint8_t, usb_vendor_read_buffer_size> buffered_bytes{};
    for (;;) {
        while (tud_vendor_available()) {
            const std::uint32_t count =
                tud_vendor_read(buffered_bytes.data(), buffered_bytes.size());
            if (count == 0U) break;
            consume_received_bytes(buffered_bytes.data(), count);
        }
        vTaskDelay(pdMS_TO_TICKS(
            firmware::application::usb_task_poll_delay_milliseconds));
    }
}

/// Handles one USB-origin file-transfer frame.
///
/// @param frame Complete decoded file-transfer frame from the USB host.
void handle_usb_file_transfer(const firmware::core::Frame& frame) {
    if (usb_file_mutex == nullptr ||
        xSemaphoreTake(usb_file_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    const std::uint64_t now =
        static_cast<std::uint64_t>(
            esp_timer_get_time() / microseconds_per_millisecond);
    if (frame.type == firmware::core::protocol::file_command) {
        const auto start = firmware::core::parse_file_transfer_start(frame.payload);
        if (!start.has_value()) {
            xSemaphoreGive(usb_file_mutex);
            return;
        }
        const bool owned_by_usb =
            shared_host_router().ownership().is_file_owner(usb_host_identity);
        const bool capacity_available =
            !usb_upload.active() && !usb_download.active() &&
            (owned_by_usb ||
             shared_host_router().ownership().claim_file(usb_host_identity));
        if (!capacity_available) {
            const std::string_view message =
                firmware::application::file_owner_limit_message;
            const firmware::core::Frame rejection{
                firmware::core::protocol::file_cancel,
                firmware::core::ByteVector(message.begin(), message.end())};
            const auto encoded = firmware::core::encode_frame(rejection);
            if (!encoded.empty()) {
                static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
            }
            xSemaphoreGive(usb_file_mutex);
            return;
        }
        const bool started = start->direction ==
                                 firmware::core::FileTransferDirection::upload
                             ? usb_upload.start(usb_host_identity, start->path,
                                                now, usb_upload_port)
                             : usb_download.start(usb_host_identity, start->path,
                                                  now, usb_download_port);
        if (!started) {
            shared_host_router().ownership().release_file();
        }
        xSemaphoreGive(usb_file_mutex);
        return;
    }
    if (usb_upload.active()) {
        usb_upload.handle(frame, now, usb_upload_port);
    }
    if (usb_download.active()) {
        usb_download.handle(frame, now, usb_download_port);
    }
    if (!usb_upload.active() && !usb_download.active() &&
        shared_host_router().ownership().is_file_owner(usb_host_identity)) {
        shared_host_router().ownership().release_file();
    }
    xSemaphoreGive(usb_file_mutex);
}

/// Queues one complete file frame for the dedicated file worker.
///
/// File handling used to run synchronously in the TinyUSB RX path. Transfers
/// can perform blocking FAT and hashing operations, which prevented TinyUSB
/// from servicing later packets and caused otherwise valid uploads to time
/// out. The bounded queue moves that work out of the callback without allowing
/// an unbounded host burst to consume memory.
///
/// @param frame Complete decoded file-transfer frame to enqueue.
/// @return `true` when queue capacity was available; otherwise `false`.
bool enqueue_usb_file_transfer(firmware::core::Frame frame) {
    if (usb_file_mutex == nullptr ||
        xSemaphoreTake(usb_file_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    const bool accepted =
        pending_usb_file_frames.size() < maximum_pending_usb_file_frames;
    if (accepted) {
        pending_usb_file_frames.push_back(std::move(frame));
    }
    xSemaphoreGive(usb_file_mutex);
    return accepted;
}

/// Removes the oldest frame waiting for the dedicated file worker.
///
/// @return The next frame, or `std::nullopt` when none is pending.
std::optional<firmware::core::Frame> dequeue_usb_file_transfer() {
    if (usb_file_mutex == nullptr ||
        xSemaphoreTake(usb_file_mutex, portMAX_DELAY) != pdTRUE) {
        return std::nullopt;
    }
    std::optional<firmware::core::Frame> frame;
    if (!pending_usb_file_frames.empty()) {
        frame = std::move(pending_usb_file_frames.front());
        pending_usb_file_frames.pop_front();
    }
    xSemaphoreGive(usb_file_mutex);
    return frame;
}

/// Processes queued file frames and polls retry and inactivity deadlines.
///
/// @param unused FreeRTOS task parameter; shared state is module-owned.
void usb_file_transfer_task(void* /* unused */) {
    for (;;) {
        if (auto frame = dequeue_usb_file_transfer(); frame.has_value()) {
            handle_usb_file_transfer(*frame);
        }
        if (xSemaphoreTake(usb_file_mutex, portMAX_DELAY) == pdTRUE) {
            const std::uint64_t now =
                static_cast<std::uint64_t>(
                    esp_timer_get_time() / microseconds_per_millisecond);
            if (usb_upload.active()) usb_upload.poll(now, usb_upload_port);
            if (usb_download.active()) {
                usb_download.poll(now, usb_download_port);
            }
            if (!usb_upload.active() && !usb_download.active() &&
                shared_host_router().ownership().is_file_owner(usb_host_identity)) {
                shared_host_router().ownership().release_file();
            }
            xSemaphoreGive(usb_file_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(file_transfer_poll_milliseconds));
    }
}

void consume_received_bytes(const std::uint8_t* bytes, std::size_t size) {
    if (bytes == nullptr || size == 0U) return;
    firmware::application::UsbReceiveStaging& staging =
        protocol_state.receive_staging();
    if (!staging.stage({bytes, size})) return;
    const auto staged = staging.take();
    for (const auto& frame : decoder.push(staged)) {
        protocol_state.valid_frame_received();
        if (frame.type == firmware::core::protocol::file_command ||
            (frame.type >= firmware::core::protocol::file_md5 &&
             frame.type <= firmware::core::protocol::file_retry)) {
            static_cast<void>(enqueue_usb_file_transfer(frame));
            continue;
        }
        if (frame.type == firmware::core::protocol::single_command &&
            !frame.payload.empty() && frame.payload.front() == '?') {
            firmware::target::RuntimeStatusAdapter status_sources(
                firmware::target::shared_host_router());
            firmware::application::AggregatedStatusService status_service(
                status_sources);
            const auto response = shared_controller_snapshots().status_reply(
                status_service.extension());
            if (response.has_value()) {
                static_cast<void>(protocol_state.transmit_queue().enqueue(
                    firmware::core::encode_frame(*response)));
            }
            continue;
        }
        if (frame.type == firmware::core::protocol::play_status) {
            const auto response = shared_play_session().status_reply();
            const auto encoded = firmware::core::encode_frame(response);
            if (!encoded.empty()) {
                static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
            }
            continue;
        }
        if (frame.type == firmware::core::protocol::general_command) {
            const auto match = firmware::core::recognize_command(frame.payload);
            if (frame.payload.size() >= 4U && frame.payload[0] == 'p' &&
                frame.payload[1] == 'l' && frame.payload[2] == 'a' &&
                frame.payload[3] == 'y') {
                const firmware::application::HostIdentity usb_identity{
                    firmware::application::HostTransport::usb, 0U, 0U};
                if (shared_host_router().ownership().claim_play(usb_identity)) {
                    auto& play_session = shared_play_session();
                    if (play_session.prepare(
                            frame.payload,
                            static_cast<std::uint64_t>(esp_timer_get_time() /
                                                       microseconds_per_millisecond),
                            usb_play_port)) {
                        const auto response = play_session.status_reply();
                        const auto encoded =
                            firmware::core::encode_frame(response);
                        if (!encoded.empty()) {
                            static_cast<void>(protocol_state.transmit_queue().enqueue(
                                encoded));
                        }
                    } else {
                        shared_host_router().ownership().release_play();
                    }
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::can_exercise) {
                const bool capacity = claim_m942_worker();
                auto* service = new firmware::application::M942ExerciseService(
                    usb_m942_port);
                if (!service->submit(usb_host_identity, frame, capacity)) {
                    delete service;
                    if (capacity) {
                        release_m942_worker();
                    }
                    continue;
                }
                if (xTaskCreate(usb_m942_task, "usb_m942",
                                usb_m942_task_stack_size, service,
                                usb_worker_priority, nullptr) != pdPASS) {
                    release_m942_worker();
                    delete service;
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::record_start ||
                match.kind == firmware::core::CommandKind::record_stop) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::serial_get ||
                match.kind == firmware::core::CommandKind::serial_set) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::system_time ||
                match.kind == firmware::core::CommandKind::clear_first_time) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::upgrade ||
                match.kind == firmware::core::CommandKind::reset) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::make_directory ||
                match.kind == firmware::core::CommandKind::remove ||
                match.kind == firmware::core::CommandKind::move ||
                match.kind == firmware::core::CommandKind::file_type) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::list) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::md5_sum) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::wlan) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::config_restore ||
                match.kind == firmware::core::CommandKind::config_default) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::config_get ||
                match.kind == firmware::core::CommandKind::config_set) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::upgrade ||
                match.kind == firmware::core::CommandKind::reset) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::diagnose) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::mock_sd_control) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::mock_nvs_control) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::mock_network_control) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
            if (match.kind == firmware::core::CommandKind::version) {
                static_cast<void>(enqueue_usb_local_command(frame));
                continue;
            }
        }
        // Apply the same transfer suppression policy used by TCP before
        // forwarding an ordinary USB-origin frame to the controller UART.
        auto& host_router = shared_host_router();
        host_router.set_controller_transfer_active(
            controller_firmware_transfer_active() ||
            controller_configuration_transfer_active() ||
            controller_factory_transfer_active());
        const auto decision = host_router.from_host(usb_host_identity, frame);
        if (decision.has(firmware::application::RouteTarget::controller)) {
            static_cast<void>(enqueue_controller_frame(frame));
        }
    }
}

}  // namespace

/// Marks native USB ready when TinyUSB reports that the device was mounted.
extern "C" void tud_mount_cb(void) {
    protocol_state.enumerated();
}

/// Clears connection and decoder state when the USB host unmounts the device.
extern "C" void tud_umount_cb(void) {
    decoder.reset();
    protocol_state.disconnected();
    tcp_router_usb_disconnected();
}

/// Accepts data reported by the TinyUSB vendor-interface receive callback.
///
/// @param index Vendor-interface index that produced the callback.
/// @param buffer Optional bytes supplied directly by TinyUSB.
/// @param size Number of readable bytes in @p buffer.
extern "C" void tud_vendor_rx_cb(uint8_t index, const uint8_t* buffer,
                                  uint16_t size) {
    if (index != primary_vendor_interface_index) return;
    // ESP-IDF TinyUSB has used both callback contracts: some configurations
    // provide bytes here, while buffered configurations only signal that the
    // FIFO is readable. Supporting both avoids silently dropping USB input
    // when the TinyUSB buffer configuration changes.
    if (buffer != nullptr && size > 0U) {
        consume_received_bytes(buffer, size);
        return;
    }
    std::array<std::uint8_t, usb_vendor_read_buffer_size> buffered_bytes{};
    while (tud_vendor_available()) {
        const std::uint32_t count =
            tud_vendor_read(buffered_bytes.data(), buffered_bytes.size());
        if (count == 0U) break;
        consume_received_bytes(buffered_bytes.data(), count);
    }
}

/// Handles a TinyUSB vendor-interface transmission notification.
///
/// Transmission progress is polled by `usb_transmit_task`, so no callback work
/// is currently required.
///
/// @param index Vendor-interface index that completed transmission.
/// @param sent_bytes Number of bytes TinyUSB reports as transmitted.
extern "C" void tud_vendor_tx_cb(uint8_t /* index */,
                                  uint32_t /* sent_bytes */) {}

bool UsbDeviceAdapter::start() {
    const tinyusb_config_t configuration{
        .device_descriptor = reinterpret_cast<const tusb_desc_device_t*>(
            device_descriptor.data()),
        .string_descriptor = string_descriptors,
        .string_descriptor_count = string_descriptor_count,
        .external_phy = false,
        .configuration_descriptor = configuration_descriptor.data(),
        .self_powered = false,
        .vbus_monitor_io = -1,
    };
    const esp_err_t result = tinyusb_driver_install(&configuration);
    if (result != ESP_OK) {
        ESP_LOGW(tag, "TinyUSB installation failed: %s", esp_err_to_name(result));
        return false;
    }
    usb_file_mutex = xSemaphoreCreateMutex();
    if (usb_file_mutex == nullptr) {
        ESP_LOGW(tag, "USB file-transfer mutex allocation failed");
        return false;
    }
    usb_local_command_mutex = xSemaphoreCreateMutex();
    if (usb_local_command_mutex == nullptr) {
        ESP_LOGW(tag, "USB local-command mutex allocation failed");
        return false;
    }
    if (xTaskCreate(usb_transmit_task, "usb_tx", usb_transmit_task_stack_size,
                    nullptr, usb_worker_priority, nullptr) != pdPASS) {
        ESP_LOGE(tag, "USB TinyUSB transmit task allocation failed");
        return false;
    }
    // Decoding a maximum-sized host frame needs more stack than the original
    // packet-sized USB traffic did. Prefer PSRAM for this blocking worker to
    // preserve internal DMA-capable memory, but retain an internal-memory
    // fallback for boards without usable PSRAM.
    BaseType_t receive_task = xTaskCreateWithCaps(
        usb_receive_task, "usb_rx", usb_blocking_worker_stack_size, nullptr,
        usb_worker_priority, nullptr,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (receive_task != pdPASS) {
        receive_task = xTaskCreate(
            usb_receive_task, "usb_rx", usb_blocking_worker_stack_size,
            nullptr, usb_worker_priority, nullptr);
    }
    // File processing needs a larger stack for FAT and hashing. Prefer PSRAM so
    // it does not exhaust internal DMA-capable memory, with the same fallback
    // needed by hardware variants where external allocation is unavailable.
    BaseType_t file_task = xTaskCreateWithCaps(
        usb_file_transfer_task, "usb_file", usb_blocking_worker_stack_size,
        nullptr, usb_worker_priority, nullptr,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (file_task != pdPASS) {
        file_task = xTaskCreate(
            usb_file_transfer_task, "usb_file", usb_blocking_worker_stack_size,
            nullptr, usb_worker_priority, nullptr);
    }
    // Runtime commands call time and NVS libc paths that require an internal
    // task stack. Keep the enlarged stack that filesystem commands need, but
    // do not place this mixed command worker in PSRAM.
    const BaseType_t local_task = xTaskCreate(
        usb_local_command_task, "usb_local", usb_blocking_worker_stack_size,
        nullptr, usb_worker_priority, nullptr);
    if (receive_task != pdPASS || file_task != pdPASS || local_task != pdPASS) {
        ESP_LOGE(tag,
                 "USB worker task allocation failed receive=%ld file=%ld local=%ld "
                 "internal_heap=%u",
                 static_cast<long>(receive_task), static_cast<long>(file_task),
                 static_cast<long>(local_task),
                 static_cast<unsigned>(
                     heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
        return false;
    }
    return true;
}

bool queue_usb_frame(const firmware::core::Frame& frame) {
    const auto encoded = firmware::core::encode_frame(frame);
    if (encoded.empty()) return false;
    return protocol_state.transmit_queue().enqueue(encoded);
}

}  // namespace firmware::target
