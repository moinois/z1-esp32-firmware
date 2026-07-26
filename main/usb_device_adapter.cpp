// Implements TinyUSB vendor callbacks over the transport-neutral USB policies.
#include "usb_device_adapter.hpp"

#include "tinyusb.h"
#include "tusb.h"
#include "class/vendor/vendor_device.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firmware/application/usb_descriptors.hpp"
#include "firmware/application/usb_protocol_state.hpp"
#include "firmware/application/usb_transmit_progress.hpp"
#include "firmware/application/recording_commands.hpp"
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
#include "firmware/application/file_upload.hpp"
#include "firmware/application/file_download.hpp"
#include "firmware/application/m942_exercise.hpp"
#include "configuration_file_store.hpp"
#include "wlan_event_adapter.hpp"
#include "wifi_diagnostic_log.hpp"

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
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <climits>
#include "esp_heap_caps.h"
#include "mbedtls/md5.h"
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
constexpr std::array<std::uint8_t, 18> device_descriptor{
    0x12U, 0x01U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0x40U,
    0x3aU, 0x30U, 0x02U, 0x40U, 0x00U, 0x01U, 0x01U, 0x02U,
    0x03U, 0x01U};
constexpr std::array<std::uint8_t, 32> configuration_descriptor{
    0x09U, 0x02U, 0x20U, 0x00U, 0x01U, 0x01U, 0x00U, 0x80U,
    0xfaU, 0x09U, 0x04U, 0x00U, 0x00U, 0x02U, 0xffU, 0x00U,
    0x00U, 0x00U, 0x07U, 0x05U, 0x01U, 0x02U, 0x40U, 0x00U,
    0x00U, 0x07U, 0x05U, 0x81U, 0x02U, 0x40U, 0x00U, 0x00U};
const char* string_descriptors[] = {"Espressif", "MakeraZ1 (USB)", "123456"};
firmware::application::UsbProtocolState protocol_state;
firmware::core::StreamDecoder decoder(firmware::core::StreamPolicy::usb());
RecordingRequestState recording_state;
firmware::application::LiveConfiguration usb_live_configuration;

// Provides POSIX-backed configuration I/O while routing replies to USB.
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

// Provides bytewise POSIX copy operations for config restore/default commands.
class UsbConfigurationFilePort final
    : public firmware::application::ConfigurationFilePort {
public:
    std::string_view active_configuration_path() const override {
        return firmware::target::active_configuration_path();
    }
    std::string_view default_configuration_path() const override {
        return "/sd/config.default";
    }
    ~UsbConfigurationFilePort() override {
        close_source();
        close_destination();
    }

    bool file_exists(std::string_view path) override {
        struct stat information{};
        return stat(std::string(path).c_str(), &information) == 0 &&
               S_ISREG(information.st_mode);
    }

    bool open_source(std::string_view path) override {
        close_source();
        source_ = std::fopen(std::string(path).c_str(), "rb");
        return source_ != nullptr;
    }

    bool open_truncated_destination(std::string_view path) override {
        close_destination();
        destination_ = std::fopen(std::string(path).c_str(), "wb");
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

// Performs blocking ESP-IDF scans and queues WLAN responses on USB.
class UsbWlanScanPort final : public firmware::application::WlanCommandPort {
public:
    void stop_scan() override {
        static_cast<void>(esp_wifi_scan_stop());
    }

    void delay_milliseconds(std::uint32_t duration) override {
        vTaskDelay(pdMS_TO_TICKS(duration));
    }

    firmware::application::WifiScanOutcome scan(
        const firmware::application::WifiScanConfig& config) override {
        wifi_scan_config_t scan_config{};
        scan_config.scan_type = config.active ? WIFI_SCAN_TYPE_ACTIVE
                                              : WIFI_SCAN_TYPE_PASSIVE;
        scan_config.show_hidden = config.include_hidden;
        scan_config.scan_time.active.min = config.active_dwell_milliseconds;
        scan_config.scan_time.active.max = config.active_dwell_milliseconds;
        scan_config.scan_time.passive = config.passive_dwell_milliseconds;
        if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
            return {false, "scan_start", {}};
        }
        std::uint16_t count = 0U;
        if (esp_wifi_scan_get_ap_num(&count) != ESP_OK) {
            return {false, "scan_count", {}};
        }
        count = static_cast<std::uint16_t>(
            count > config.maximum_observations ? config.maximum_observations
                                                 : count);
        std::vector<wifi_ap_record_t> records(count);
        if (count > 0U &&
            esp_wifi_scan_get_ap_records(&count, records.data()) != ESP_OK) {
            return {false, "scan_records", {}};
        }
        std::vector<firmware::core::WifiObservation> observations;
        observations.reserve(count);
        for (std::uint16_t index = 0U; index < count; ++index) {
            const wifi_ap_record_t& record = records[index];
            std::size_t length = 0U;
            while (length < sizeof(record.ssid) && record.ssid[length] != 0U) {
                ++length;
            }
            observations.push_back({
                firmware::core::ByteVector(record.ssid, record.ssid + length),
                record.rssi,
                static_cast<std::uint8_t>(record.authmode)});
        }
        return {true, {}, std::move(observations)};
    }

    std::string connected_ssid() const override {
        wifi_ap_record_t record{};
        if (esp_wifi_sta_get_ap_info(&record) != ESP_OK) return {};
        std::size_t length = 0U;
        while (length < sizeof(record.ssid) && record.ssid[length] != 0U) {
            ++length;
        }
        return std::string(reinterpret_cast<const char*>(record.ssid), length);
    }

    void send(firmware::core::Frame frame) override {
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }
};

UsbWlanScanPort wlan_scan_port;

// Converts an ESP-IDF result into the transport-neutral station result.
firmware::application::StationApiResult api_result(esp_err_t result,
                                                    const char* operation) {
    return result == ESP_OK
               ? firmware::application::StationApiResult{true, {}}
               : firmware::application::StationApiResult{false, operation};
}

// Implements manual station operations using ESP-IDF and shared NVS storage.
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
        if (duration >= 1000U) {
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
        file_ = std::fopen(std::string(path).c_str(), "rb");
        if (file_ == nullptr || std::fseek(file_, 0L, SEEK_END) != 0) {
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
        FILE* input = std::fopen(std::string(*cache).c_str(), "rb");
        if (input == nullptr) return std::nullopt;
        std::uint8_t bytes[63]{};
        const std::size_t count = std::fread(bytes, 1U, sizeof(bytes), input);
        std::fclose(input);
        return firmware::core::extract_cached_md5({bytes, count});
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
class UsbFileUploadPort final : public firmware::application::FileUploadPort {
public:
    ~UsbFileUploadPort() override { close_files(); }

    void prepare_cache_paths(const firmware::core::FileCachePaths& paths) override {
        if (paths.md5_path.has_value()) create_parent_directories(*paths.md5_path, 0777U);
        if (paths.compressed_path.has_value()) {
            create_parent_directories(*paths.compressed_path, 0777U);
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
        primary_ = std::fopen(std::string(path).c_str(), "wb");
        return primary_ != nullptr;
    }

    bool open_md5(std::string_view path) override {
        md5_ = std::fopen(std::string(path).c_str(), "wb");
        return md5_ != nullptr;
    }

    bool write_primary(firmware::core::BytesView data) override {
        return write_all(primary_, data);
    }

    bool write_md5(firmware::core::BytesView data) override {
        return write_all(md5_, data);
    }

    void close_files() override {
        if (primary_ != nullptr) std::fclose(primary_);
        if (md5_ != nullptr) std::fclose(md5_);
        primary_ = nullptr;
        md5_ = nullptr;
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
        return rename(std::string(source).c_str(),
                      std::string(destination).c_str()) == 0;
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
    static bool write_all(FILE* file, firmware::core::BytesView data) {
        if (file == nullptr) return false;
        std::size_t written = 0U;
        while (written < data.size()) {
            const std::size_t count =
                std::fwrite(data.data() + written, 1U, data.size() - written,
                            file);
            if (count == 0U) return false;
            written += count;
        }
        return true;
    }

    FILE* primary_ = nullptr;
    FILE* md5_ = nullptr;
};

UsbFileUploadPort usb_upload_port;
firmware::application::FileUpload usb_upload;
const firmware::application::HostIdentity usb_host_identity{
    firmware::application::HostTransport::usb, 0U, 0U};

// Implements bounded POSIX reads and MD5 lookup for USB downloads.
class UsbFileDownloadPort final : public firmware::application::FileDownloadPort {
public:
    ~UsbFileDownloadPort() override { close_file(); }

    void prepare_cache_paths(const firmware::core::FileCachePaths& paths) override {
        if (paths.md5_path.has_value()) usb_upload_port.create_parent_directories(
            *paths.md5_path, 0777U);
        if (paths.compressed_path.has_value()) usb_upload_port.create_parent_directories(
            *paths.compressed_path, 0777U);
    }

    std::optional<std::string> calculate_md5(std::string_view path) override {
        FILE* input = std::fopen(std::string(path).c_str(), "rb");
        if (input == nullptr) return std::nullopt;
        mbedtls_md5_context context;
        mbedtls_md5_init(&context);
        mbedtls_md5_starts(&context);
        std::uint8_t buffer[1024];
        while (const std::size_t count =
                   std::fread(buffer, 1U, sizeof(buffer), input)) {
            mbedtls_md5_update(&context, buffer, count);
        }
        if (std::ferror(input) != 0) {
            std::fclose(input);
            mbedtls_md5_free(&context);
            return std::nullopt;
        }
        std::uint8_t digest[16];
        mbedtls_md5_finish(&context, digest);
        mbedtls_md5_free(&context);
        std::fclose(input);
        static constexpr char hex[] = "0123456789abcdef";
        std::string result(32U, '0');
        for (std::size_t index = 0U; index < 16U; ++index) {
            result[index * 2U] = hex[digest[index] >> 4U];
            result[index * 2U + 1U] = hex[digest[index] & 0x0fU];
        }
        return result;
    }

    std::optional<firmware::core::ByteVector> read_cache(
        std::string_view path, std::size_t maximum_size) override {
        FILE* input = std::fopen(std::string(path).c_str(), "rb");
        if (input == nullptr) return std::nullopt;
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
        close_file();
        file_ = std::fopen(std::string(path).c_str(), "rb");
        if (file_ == nullptr || std::fseek(file_, 0L, SEEK_END) != 0) {
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

    std::optional<firmware::core::ByteVector> read_file(
        std::uint64_t offset, std::size_t maximum_size) override {
        if (file_ == nullptr || offset > static_cast<std::uint64_t>(LONG_MAX) ||
            std::fseek(file_, static_cast<long>(offset), SEEK_SET) != 0) {
            return std::nullopt;
        }
        firmware::core::ByteVector data(maximum_size);
        const std::size_t count = std::fread(data.data(), 1U, maximum_size, file_);
        if (std::ferror(file_) != 0) return std::nullopt;
        data.resize(count);
        return data;
    }

    bool allocate_response_workspace(std::size_t size) override {
        void* workspace = heap_caps_malloc(size, MALLOC_CAP_8BIT);
        if (workspace == nullptr) return false;
        heap_caps_free(workspace);
        return true;
    }

    void close_file() override {
        if (file_ != nullptr) std::fclose(file_);
        file_ = nullptr;
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
    FILE* file_ = nullptr;
};

UsbFileDownloadPort usb_download_port;
firmware::application::FileDownload usb_download;

// Adapts M942 CAN exercise responses and remote SDO access to USB.
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
        return static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL);
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
void usb_m942_task(void* parameter) {
    auto* service = static_cast<firmware::application::M942ExerciseService*>(
        parameter);
    service->run();
    release_m942_worker();
    delete service;
    vTaskDelete(nullptr);
}
SemaphoreHandle_t usb_file_mutex = nullptr;

class UsbSerialPort final : public firmware::application::SerialNumberPort {
public:
    bool admit_operation(std::uint32_t wait_milliseconds) override {
        return admit_runtime_operation(wait_milliseconds);
    }

    firmware::application::SerialNumberRead read_serial(
        std::string_view name_space, std::string_view key) override {
        NvsKeyValueAdapter nvs;
        const auto result = nvs.read_string(name_space, key);
        if (result.state == NvsReadState::found) {
            return {firmware::application::SerialNumberReadResult::success,
                    result.value};
        }
        if (result.state == NvsReadState::missing) {
            return {firmware::application::SerialNumberReadResult::missing_key,
                    {}};
        }
        return {firmware::application::SerialNumberReadResult::failure, {}};
    }

    bool write_serial(std::string_view name_space, std::string_view key,
                      std::string_view value) override {
        NvsKeyValueAdapter nvs;
        return nvs.write_string(name_space, key, value);
    }

    void complete_operation() override {
        complete_runtime_operation();
    }

    void send_response(std::uint8_t type, std::string_view payload) override {
        const firmware::core::Frame response{
            type, firmware::core::ByteVector(payload.begin(), payload.end())};
        const auto encoded = firmware::core::encode_frame(response);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }
};

UsbSerialPort serial_port;

class UsbRuntimePort final : public firmware::application::RuntimeCommandPort {
public:
    bool admit_operation(std::uint32_t wait_milliseconds) override {
        return admit_runtime_operation(wait_milliseconds);
    }

    bool open_namespace(std::string_view name_space) override {
        name_space_ = std::string(name_space);
        return true;
    }

    firmware::application::RuntimeSignedRead read_first_boot(
        std::string_view key) override {
        NvsKeyValueAdapter nvs;
        const auto value = nvs.read_u64_state(name_space_, key);
        if (value.state == NvsReadState::found) {
            return {firmware::application::RuntimeValueResult::success,
                    static_cast<std::int64_t>(value.value)};
        }
        if (value.state == NvsReadState::missing) {
            return {firmware::application::RuntimeValueResult::missing, 0};
        }
        return {firmware::application::RuntimeValueResult::failure, 0};
    }

    std::optional<std::uint64_t> read_counter(std::string_view key) override {
        NvsKeyValueAdapter nvs;
        return nvs.read_u64(name_space_, key);
    }

    std::optional<std::string> format_utc_minute(
        std::int64_t seconds) override {
        const time_t value = static_cast<time_t>(seconds);
        std::tm utc{};
        if (gmtime_r(&value, &utc) == nullptr) return std::nullopt;
        char buffer[32];
        if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M UTC", &utc) == 0U) {
            return std::nullopt;
        }
        return std::string(buffer);
    }

    firmware::application::RuntimeEraseResult erase_first_boot(
        std::string_view name_space, std::string_view key) override {
        NvsKeyValueAdapter nvs;
        const auto result = nvs.erase_key(name_space, key);
        if (result == NvsReadState::found) {
            return firmware::application::RuntimeEraseResult::success;
        }
        if (result == NvsReadState::missing) {
            return firmware::application::RuntimeEraseResult::missing;
        }
        return firmware::application::RuntimeEraseResult::failure;
    }

    void complete_operation() override {
        complete_runtime_operation();
    }

    void send_response(std::uint8_t type, std::string_view payload) override {
        const firmware::core::Frame response{
            type, firmware::core::ByteVector(payload.begin(), payload.end())};
        const auto encoded = firmware::core::encode_frame(response);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }

private:
    std::string name_space_;
};

UsbRuntimePort runtime_port;

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
            if (name == "." || name == "..") continue;
            remove_usb_tree(path + "/" + name);
        }
        closedir(directory);
    }
    static_cast<void>(rmdir(path.c_str()));
}

class UsbFilesystemPort final
    : public firmware::application::FilesystemCommandPort {
public:
    bool create_directory(std::string_view path, std::uint32_t mode) override {
        const std::string value(path);
        return mkdir(value.c_str(), static_cast<mode_t>(mode)) == 0 ||
               errno == EEXIST;
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
        return rename(old_path.c_str(), new_path.c_str()) == 0;
    }

    void send(firmware::core::Frame frame) override {
        const auto encoded = firmware::core::encode_frame(frame);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }
};

UsbFilesystemPort filesystem_port;

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

class UsbDirectoryPort final : public firmware::application::DirectoryListPort {
public:
    std::optional<std::vector<firmware::application::DirectoryEntry>>
    list_directory(std::string_view path) override {
        const std::string root(path);
        DIR* directory = opendir(root.c_str());
        if (directory == nullptr) return std::nullopt;
        std::vector<firmware::application::DirectoryEntry> entries;
        while (const dirent* item = readdir(directory)) {
            const std::string name(item->d_name);
            if (name == "." || name == "..") continue;
            struct stat information{};
            const std::string full_path = root + "/" + name;
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

class UsbHashPort final : public firmware::application::FileHashPort {
public:
    firmware::application::FileHashPathState inspect_path(
        std::string_view path) override {
        struct stat information{};
        if (stat(std::string(path).c_str(), &information) != 0) {
            return firmware::application::FileHashPathState::missing;
        }
        return S_ISREG(information.st_mode)
                   ? firmware::application::FileHashPathState::regular_file
                   : firmware::application::FileHashPathState::not_regular;
    }

    std::optional<std::string> calculate_md5(std::string_view path,
                                             std::size_t block_size) override {
        std::FILE* file = std::fopen(std::string(path).c_str(), "rb");
        if (file == nullptr || block_size == 0U) {
            if (file != nullptr) std::fclose(file);
            return std::nullopt;
        }
        std::vector<std::uint8_t> buffer(block_size);
        mbedtls_md5_context context;
        mbedtls_md5_init(&context);
        mbedtls_md5_starts(&context);
        while (const std::size_t count = std::fread(buffer.data(), 1U, buffer.size(), file)) {
            mbedtls_md5_update(&context, buffer.data(), count);
        }
        if (std::ferror(file) != 0) {
            std::fclose(file);
            mbedtls_md5_free(&context);
            return std::nullopt;
        }
        std::uint8_t digest[16];
        mbedtls_md5_finish(&context, digest);
        mbedtls_md5_free(&context);
        std::fclose(file);
        static constexpr char hex[] = "0123456789abcdef";
        std::string result(32U, '0');
        for (std::size_t index = 0U; index < 16U; ++index) {
            result[index * 2U] = hex[digest[index] >> 4U];
            result[index * 2U + 1U] = hex[digest[index] & 0x0fU];
        }
        return result;
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

// Forwards USB bytes from TinyUSB into the transport-neutral frame decoder.
void consume_received_bytes(const std::uint8_t* bytes, std::size_t size);

// Enqueues one USB-local frame while serializing callback/task access.
bool enqueue_usb_local_command(const firmware::core::Frame& frame) {
    if (usb_local_command_mutex == nullptr ||
        xSemaphoreTake(usb_local_command_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    const bool queued = usb_local_commands.enqueue(frame);
    xSemaphoreGive(usb_local_command_mutex);
    return queued;
}

// Removes one USB-local frame while serializing callback/task access.
std::optional<firmware::core::Frame> dequeue_usb_local_command() {
    if (usb_local_command_mutex == nullptr ||
        xSemaphoreTake(usb_local_command_mutex, portMAX_DELAY) != pdTRUE) {
        return std::nullopt;
    }
    auto frame = usb_local_commands.dequeue();
    xSemaphoreGive(usb_local_command_mutex);
    return frame;
}

// Handles short USB-local commands outside the TinyUSB receive callback.
void usb_local_command_task(void*) {
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
            if (match.kind == firmware::core::CommandKind::make_directory) {
                firmware::application::FilesystemCommands::make_directory(
                    command_frame->payload, filesystem_port);
            } else if (match.kind == firmware::core::CommandKind::remove) {
                firmware::application::FilesystemCommands::remove(
                    command_frame->payload, filesystem_port);
            } else if (match.kind == firmware::core::CommandKind::move) {
                firmware::application::FilesystemCommands::move(
                    command_frame->payload, filesystem_port);
            } else {
                firmware::application::FilesystemCommands::file_type(
                    filesystem_port);
            }
        } else if (match.kind == firmware::core::CommandKind::list) {
            firmware::application::DirectoryListing::execute(
                command_frame->payload, directory_port);
        } else if (match.kind == firmware::core::CommandKind::md5_sum) {
            firmware::application::FileHashCommand::execute(
                command_frame->payload, hash_port);
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
            if (match.kind == firmware::core::CommandKind::config_get) {
                firmware::application::ConfigurationGet::execute(
                    command_frame->payload, usb_live_configuration,
                    configuration_port);
            } else {
                firmware::application::ConfigurationSet::execute(
                    command_frame->payload, usb_live_configuration,
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
        } else if (match.kind == firmware::core::CommandKind::version) {
            const auto response = shared_controller_snapshots().version_reply();
            static_cast<void>(protocol_state.transmit_queue().enqueue(
                firmware::core::encode_frame(response)));
        }
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}

void usb_transmit_task(void*) {
    firmware::application::UsbTransmitProgress progress;
    const firmware::core::ByteVector* tracked_frame = nullptr;
    for (;;) {
        if (protocol_state.can_send()) {
            const auto* frame = protocol_state.transmit_queue().front();
            if (frame != tracked_frame) {
                tracked_frame = frame;
                progress.begin(static_cast<std::uint64_t>(
                    esp_timer_get_time() / 1000LL));
            }
            if (frame != nullptr && tud_vendor_write_available() >= frame->size()) {
                const std::uint32_t written =
                    tud_vendor_write(frame->data(), frame->size());
                if (written == frame->size()) {
                    tud_vendor_flush();
                    protocol_state.transmit_queue().pop_front();
                    progress.clear();
                    tracked_frame = nullptr;
                } else if (written > 0U) {
                    progress.record_progress(static_cast<std::uint64_t>(
                        esp_timer_get_time() / 1000LL));
                }
            }
            if (frame != nullptr && progress.expired(static_cast<std::uint64_t>(
                    esp_timer_get_time() / 1000LL))) {
                protocol_state.transmit_queue().pop_front();
                progress.clear();
                tracked_frame = nullptr;
            }
        } else {
            tracked_frame = nullptr;
            progress.clear();
        }
        vTaskDelay(pdMS_TO_TICKS(
            firmware::application::usb_task_poll_delay_milliseconds));
    }
}

// Polls the buffered TinyUSB vendor FIFO so incoming frames are consumed
// consistently across the ESP-IDF TinyUSB buffer configurations.
void usb_receive_task(void*) {
    std::array<std::uint8_t, 512> buffered_bytes{};
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

// Handles one USB-origin file transfer frame and releases ownership on completion.
void handle_usb_file_transfer(const firmware::core::Frame& frame) {
    if (usb_file_mutex == nullptr ||
        xSemaphoreTake(usb_file_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    const std::uint64_t now =
        static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL);
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

// Polls USB upload/download inactivity and retry deadlines independently of RX callbacks.
void usb_file_transfer_task(void*) {
    for (;;) {
        if (xSemaphoreTake(usb_file_mutex, portMAX_DELAY) == pdTRUE) {
            const std::uint64_t now =
                static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL);
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
        vTaskDelay(pdMS_TO_TICKS(50U));
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
            handle_usb_file_transfer(frame);
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
                                                       1000LL),
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
                if (xTaskCreate(usb_m942_task, "usb_m942", 6144U, service,
                                4U, nullptr) != pdPASS) {
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

extern "C" void tud_mount_cb(void) {
    protocol_state.enumerated();
}

extern "C" void tud_umount_cb(void) {
    decoder.reset();
    protocol_state.disconnected();
    tcp_router_usb_disconnected();
}

extern "C" void tud_vendor_rx_cb(uint8_t index, const uint8_t* buffer,
                                  uint16_t size) {
    if (index != 0U) return;
    if (buffer != nullptr && size > 0U) {
        consume_received_bytes(buffer, size);
        return;
    }
    std::array<std::uint8_t, 512> buffered_bytes{};
    while (tud_vendor_available()) {
        const std::uint32_t count =
            tud_vendor_read(buffered_bytes.data(), buffered_bytes.size());
        if (count == 0U) break;
        consume_received_bytes(buffered_bytes.data(), count);
    }
}

extern "C" void tud_vendor_tx_cb(uint8_t, uint32_t) {}

bool UsbDeviceAdapter::start() {
    const tinyusb_config_t configuration{
        .device_descriptor = reinterpret_cast<const tusb_desc_device_t*>(
            device_descriptor.data()),
        .string_descriptor = string_descriptors,
        .string_descriptor_count = 3,
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
    xTaskCreate(usb_transmit_task, "usb_tx", 4096U, nullptr, 4U, nullptr);
    xTaskCreate(usb_receive_task, "usb_rx", 4096U, nullptr, 4U, nullptr);
    xTaskCreate(usb_file_transfer_task, "usb_file", 4096U, nullptr, 4U, nullptr);
    xTaskCreate(usb_local_command_task, "usb_local", 4096U, nullptr, 4U, nullptr);
    return true;
}

bool queue_usb_frame(const firmware::core::Frame& frame) {
    const auto encoded = firmware::core::encode_frame(frame);
    if (encoded.empty()) return false;
    return protocol_state.transmit_queue().enqueue(encoded);
}

}  // namespace firmware::target
