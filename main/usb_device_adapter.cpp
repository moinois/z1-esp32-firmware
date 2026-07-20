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
#include "firmware/application/filesystem_commands.hpp"
#include "firmware/application/directory_listing.hpp"
#include "firmware/application/file_hash_command.hpp"
#include "firmware/application/configuration_get.hpp"
#include "firmware/application/configuration_set.hpp"
#include "firmware/application/configuration_files.hpp"
#include "firmware/application/wlan_command.hpp"
#include "firmware/application/wlan_request.hpp"
#include "nvs_key_value_adapter.hpp"
#include "wifi_persistence_constants.hpp"
#include "runtime_operation_capacity.hpp"
#include "recording_request_state.hpp"
#include "firmware/core/text.hpp"
#include "firmware/core/frame.hpp"
#include "controller_command_loop.hpp"
#include "tcp_control_adapter.hpp"
#include "tcp_discovery_adapter.hpp"
#include "firmware_update_adapter.hpp"
#include "firmware/application/controller_snapshots.hpp"
#include "runtime_status_adapter.hpp"
#include "firmware/application/runtime_status.hpp"

#include <array>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <optional>
#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
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
    std::optional<std::vector<firmware::core::ByteVector>> read_chunks(
        std::string_view path, std::size_t maximum_chunk_size) override {
        if (maximum_chunk_size == 0U) return std::nullopt;
        FILE* file = std::fopen(std::string(path).c_str(), "rb");
        if (file == nullptr) return std::nullopt;
        std::vector<firmware::core::ByteVector> chunks;
        for (;;) {
            firmware::core::ByteVector chunk(maximum_chunk_size);
            const std::size_t count =
                std::fread(chunk.data(), 1U, chunk.size(), file);
            if (std::ferror(file) != 0) {
                std::fclose(file);
                return std::nullopt;
            }
            if (count == 0U) break;
            chunk.resize(count);
            chunks.push_back(std::move(chunk));
        }
        std::fclose(file);
        return chunks;
    }

    std::optional<std::string> read_active_text(std::string_view path) override {
        const auto chunks = read_chunks(path, 512U);
        if (!chunks.has_value()) return std::nullopt;
        std::string content;
        for (const auto& chunk : *chunks) {
            content.append(reinterpret_cast<const char*>(chunk.data()),
                           chunk.size());
        }
        return content;
    }

    std::optional<std::vector<std::string>> read_sd_lines(
        std::string_view path) override {
        const auto content = read_active_text(path);
        if (!content.has_value()) return std::nullopt;
        std::vector<std::string> lines;
        std::size_t start = 0U;
        while (start <= content->size()) {
            const std::size_t end = content->find('\n', start);
            const std::size_t limit =
                end == std::string::npos ? content->size() : end;
            lines.push_back(content->substr(start, limit - start));
            if (end == std::string::npos) break;
            start = end + 1U;
        }
        return lines;
    }

    bool write_temporary(std::string_view path, std::string_view content) override {
        FILE* file = std::fopen(std::string(path).c_str(), "wb");
        if (file == nullptr) return false;
        const bool written = std::fwrite(content.data(), 1U, content.size(), file) ==
                             content.size();
        const bool flushed = std::fflush(file) == 0;
        const bool closed = std::fclose(file) == 0;
        return written && flushed && closed;
    }

    bool unlink_active(std::string_view path) override {
        return unlink(std::string(path).c_str()) == 0;
    }

    bool rename_temporary(std::string_view source,
                          std::string_view destination) override {
        return rename(std::string(source).c_str(),
                      std::string(destination).c_str()) == 0;
    }

    void remove_temporary(std::string_view path) override {
        static_cast<void>(unlink(std::string(path).c_str()));
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
        wifi_config.sta.scan_method = WIFI_FAST_SCAN;
        wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        wifi_config.sta.threshold.authmode = static_cast<wifi_auth_mode_t>(
            configuration.minimum_authentication_mode);
        const esp_err_t mode_result = esp_wifi_set_mode(WIFI_MODE_STA);
        if (mode_result != ESP_OK) return api_result(mode_result, "set_mode");
        return api_result(esp_wifi_set_config(WIFI_IF_STA, &wifi_config),
                          "set_config");
    }

    firmware::application::StationApiResult request_connect() override {
        return api_result(esp_wifi_connect(), "connect");
    }

    void delay_milliseconds(std::uint32_t duration) override {
        vTaskDelay(pdMS_TO_TICKS(duration));
    }

    firmware::application::StationSnapshot station_snapshot() const override {
        wifi_ap_record_t access_point{};
        if (esp_wifi_sta_get_ap_info(&access_point) != ESP_OK) return {};
        const std::string ssid(reinterpret_cast<const char*>(access_point.ssid));
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif == nullptr) {
            return {firmware::application::StationConnectionState::associated,
                    ssid, {}};
        }
        esp_netif_ip_info_t ip_info{};
        if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK ||
            ip_info.ip.addr == 0U) {
            return {firmware::application::StationConnectionState::associated,
                    ssid, {}};
        }
        char address[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &ip_info.ip.addr, address, sizeof(address));
        return {firmware::application::StationConnectionState::address_ready,
                ssid, address};
    }

    firmware::application::StationApiResult save_credentials(
        std::string_view ssid, std::string_view password) override {
        NvsKeyValueAdapter nvs;
        if (!nvs.write_string(wifi_persistence::name_space,
                              wifi_persistence::ssid_key, ssid)) {
            return {false, "save_ssid"};
        }
        if (!nvs.write_string(wifi_persistence::name_space,
                              wifi_persistence::password_key, password)) {
            return {false, "save_password"};
        }
        return {true, {}};
    }
};

// Routes WLAN connection responses to the USB transmit queue.
class UsbWlanResponsePort final
    : public firmware::application::WlanConnectionResponsePort {
public:
    void send(firmware::core::Frame frame) override {
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
        vTaskDelay(pdMS_TO_TICKS(1U));
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
        if (frame.type == firmware::core::protocol::general_command) {
            const auto match = firmware::core::recognize_command(frame.payload);
            if (match.kind == firmware::core::CommandKind::record_start ||
                match.kind == firmware::core::CommandKind::record_stop) {
                const auto result = firmware::application::handle_recording_command(
                    match.kind, recording_state.requested());
                recording_state.set_requested(result.requested);
                const auto response = firmware::core::encode_frame(result.response);
                if (!response.empty()) {
                    static_cast<void>(protocol_state.transmit_queue().enqueue(response));
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::serial_get ||
                match.kind == firmware::core::CommandKind::serial_set) {
                const std::string_view command(
                    reinterpret_cast<const char*>(frame.payload.data()),
                    frame.payload.size());
                firmware::application::SerialNumberService service(serial_port);
                if (match.kind == firmware::core::CommandKind::serial_get) {
                    service.handle_get(command);
                } else {
                    service.handle_set(command);
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::system_time ||
                match.kind == firmware::core::CommandKind::clear_first_time) {
                const std::string_view command(
                    reinterpret_cast<const char*>(frame.payload.data()),
                    frame.payload.size());
                firmware::application::RuntimeCommandService service(runtime_port);
                if (match.kind == firmware::core::CommandKind::system_time) {
                    service.handle_system_time(command);
                } else {
                    service.handle_clear_first_boot(command);
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::make_directory ||
                match.kind == firmware::core::CommandKind::remove ||
                match.kind == firmware::core::CommandKind::move ||
                match.kind == firmware::core::CommandKind::file_type) {
                if (match.kind == firmware::core::CommandKind::make_directory) {
                    firmware::application::FilesystemCommands::make_directory(
                        frame.payload, filesystem_port);
                } else if (match.kind == firmware::core::CommandKind::remove) {
                    firmware::application::FilesystemCommands::remove(
                        frame.payload, filesystem_port);
                } else if (match.kind == firmware::core::CommandKind::move) {
                    firmware::application::FilesystemCommands::move(
                        frame.payload, filesystem_port);
                } else {
                    firmware::application::FilesystemCommands::file_type(
                        filesystem_port);
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::list) {
                firmware::application::DirectoryListing::execute(
                    frame.payload, directory_port);
                continue;
            }
            if (match.kind == firmware::core::CommandKind::md5_sum) {
                firmware::application::FileHashCommand::execute(
                    frame.payload, hash_port);
                continue;
            }
            if (match.kind == firmware::core::CommandKind::wlan) {
                const std::string command(
                    reinterpret_cast<const char*>(frame.payload.data()),
                    frame.payload.size());
                const auto request =
                    firmware::application::parse_wlan_request(command);
                if (request.kind == firmware::application::WlanRequestKind::scan) {
                    firmware::application::WlanScanCommand::execute(
                        wlan_scan_port);
                } else if (request.kind ==
                           firmware::application::WlanRequestKind::disconnect) {
                    firmware::application::WlanConnectionCommand::disconnect(
                        usb_station_runtime, wlan_station_port,
                        wlan_response_port);
                } else {
                    firmware::application::WlanConnectionCommand::connect(
                        usb_station_runtime, wlan_station_port,
                        wlan_response_port, request.ssid, request.password);
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::config_restore ||
                match.kind == firmware::core::CommandKind::config_default) {
                if (match.kind == firmware::core::CommandKind::config_restore) {
                    firmware::application::ConfigurationFiles::restore(
                        configuration_file_port);
                } else {
                    firmware::application::ConfigurationFiles::save_default(
                        configuration_file_port);
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::config_get ||
                match.kind == firmware::core::CommandKind::config_set) {
                if (match.kind == firmware::core::CommandKind::config_get) {
                    firmware::application::ConfigurationGet::execute(
                        frame.payload, usb_live_configuration, configuration_port);
                } else {
                    firmware::application::ConfigurationSet::execute(
                        frame.payload, usb_live_configuration, configuration_port);
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::upgrade ||
                match.kind == firmware::core::CommandKind::reset) {
                request_firmware_update_processing();
                continue;
            }
            if (match.kind == firmware::core::CommandKind::diagnose) {
                const auto response =
                    shared_controller_snapshots().diagnostic_reply(0);
                if (response.has_value()) {
                    static_cast<void>(protocol_state.transmit_queue().enqueue(
                        firmware::core::encode_frame(*response)));
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::version) {
                const auto response =
                    shared_controller_snapshots().version_reply();
                static_cast<void>(protocol_state.transmit_queue().enqueue(
                    firmware::core::encode_frame(response)));
                continue;
            }
        }
        // Forward complete USB frames through the controller-owned UART path.
        // Local USB command routing will be added without changing this boundary.
        static_cast<void>(enqueue_controller_frame(frame));
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

extern "C" void tud_vendor_rx_cb(uint8_t index, const uint8_t*, uint16_t) {
    if (index != 0U) return;
    std::array<std::uint8_t, 512> buffer{};
    const std::uint32_t count = tud_vendor_read(buffer.data(), buffer.size());
    consume_received_bytes(buffer.data(), count);
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
    xTaskCreate(usb_transmit_task, "usb_tx", 4096U, nullptr, 4U, nullptr);
    return true;
}

bool queue_usb_frame(const firmware::core::Frame& frame) {
    const auto encoded = firmware::core::encode_frame(frame);
    if (encoded.empty()) return false;
    return protocol_state.transmit_queue().enqueue(encoded);
}

}  // namespace firmware::target
