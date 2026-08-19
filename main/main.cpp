/** @file @brief Starts the mainboard services in the normative order defined by BOOT-010 through BOOT-019. */
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "canopen_target_service.hpp"
#include "http_server_adapter.hpp"
#include "camera_hardware_adapter.hpp"
#include "web_volume_adapter.hpp"
#include "storage_retention_adapter.hpp"
#include "hardware_adapter_factory.hpp"
#include "sd_storage_adapter.hpp"
#include "controller_command_loop.hpp"
#include "tcp_control_adapter.hpp"
#include "tcp_discovery_adapter.hpp"
#include "wlan_event_adapter.hpp"
#include "connectivity_startup_adapter.hpp"
#include "access_point_command_adapter.hpp"
#include "automatic_connection_adapter.hpp"
#include "blufi_lifecycle_adapter.hpp"
#include "blufi_provisioning_adapter.hpp"
#include "blufi_callback_adapter.hpp"
#include "firmware_update_adapter.hpp"
#include "diagnostic_capture_adapter.hpp"
#include "wifi_diagnostic_log.hpp"
#include "runtime_counter_task.hpp"
#include "heartbeat_adapter.hpp"
#include "usb_device_adapter.hpp"
#include "recording_task_adapter.hpp"
#include "configuration_file_store.hpp"

#include "application/web/web_volume_startup.hpp"
#include "application/connectivity/connectivity_startup.hpp"
#include "application/runtime/persistent_store_initialization.hpp"
#include "core/network/network_policy.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace {
// The virtual adapter call chain and ESP-IDF GPIO logging exceed the original
// 2048-byte task stack during physical boot, causing a watchdog reset loop.
constexpr std::uint32_t heartbeat_stack_size = 4096U;
constexpr UBaseType_t heartbeat_priority = 3U;
constexpr char tag[] = "MAIN";

// Loads the optional machine-name configuration and derives the MAC fallback.
std::string configured_machine_name() {
    const std::vector<std::string> lines =
        firmware::target::ConfigurationFileStore{}.read_lines();
    std::array<std::uint8_t, 6U> station_mac{};
    if (esp_read_mac(station_mac.data(), ESP_MAC_WIFI_STA) != ESP_OK) {
        station_mac.fill(0U);
    }
    return firmware::core::derive_machine_name(lines, station_mac);
}

class PersistentStoreInitializationAdapter final
    : public firmware::application::PersistentStoreInitializationPort {
public:
    firmware::application::PersistentStoreInitializationResult initialize() override {
        last_result_ = nvs_flash_init();
        if (last_result_ == ESP_OK) {
            return firmware::application::PersistentStoreInitializationResult::success;
        }
        if (last_result_ == ESP_ERR_NVS_NO_FREE_PAGES) {
            return firmware::application::
                PersistentStoreInitializationResult::exhausted_pages;
        }
        if (last_result_ == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            return firmware::application::
                PersistentStoreInitializationResult::incompatible_version;
        }
        return firmware::application::
            PersistentStoreInitializationResult::other_failure;
    }

    bool erase() override { return nvs_flash_erase() == ESP_OK; }

    void report_exhausted_recovery() override {
        ESP_LOGW(tag, "NVS分区需要擦除，正在擦除...");
    }

    void report_general_recovery() override {
        ESP_LOGE(tag, "NVS初始化失败: %s (0x%x)",
                 esp_err_to_name(last_result_),
                 static_cast<unsigned>(last_result_));
        ESP_LOGW(tag, "尝试擦除NVS分区并重新初始化...");
    }

private:
    esp_err_t last_result_ = ESP_OK;
};

// Toggles the active-high heartbeat output once per second for the lifetime of the firmware.
void heartbeat_task(void*) {
    firmware::target::EspHeartbeatAdapter port;
    firmware::application::HeartbeatService service(port);
    if (!service.start()) {
        vTaskDelete(nullptr);
        return;
    }
    for (;;) {
        service.run_cycle();
    }
}

// Starts the nonfatal heartbeat service and leaves later services independent of its result.
void start_heartbeat() {
    static_cast<void>(xTaskCreate(heartbeat_task, "heartbeat",
                                  heartbeat_stack_size, nullptr,
                                  heartbeat_priority, nullptr));
}
}  // namespace

extern "C" void app_main() {
    ESP_LOGW(tag, "Reset reason=%d", static_cast<int>(esp_reset_reason()));
    PersistentStoreInitializationAdapter persistent_store;
    if (!firmware::application::initialize_persistent_store(persistent_store)) {
        ESP_LOGE(tag, "NVS初始化仍然失败，系统无法继续运行");
        ESP_LOGE(tag, "Restarting because NVS initialization failed");
        esp_restart();
        return;
    }
    ESP_LOGI(tag, "NVS初始化成功");
    const std::string persisted_wifi_log =
        firmware::target::wifi_diagnostic_log().read();
    if (!persisted_wifi_log.empty()) {
        ESP_LOGI(tag, "Persistent Wi-Fi diagnostic log:\n%s",
                 persisted_wifi_log.c_str());
    }
    start_heartbeat();
    if (esp_netif_init() != ESP_OK || esp_event_loop_create_default() != ESP_OK) {
        ESP_LOGE(tag, "Restarting because network event initialization failed");
        esp_restart();
    }
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wifi_config) != ESP_OK) {
        ESP_LOGE(tag, "Restarting because Wi-Fi initialization failed");
        esp_restart();
    }
    // Observe the complete Wi-Fi lifecycle, including the initial STA start.
    static firmware::target::WlanEventAdapter wlan_events;
    wlan_events.start();
    // Install capture before storage startup so early diagnostics are retained.
    static firmware::target::DiagnosticCaptureAdapter diagnostic_capture;
    diagnostic_capture.start();
    // Construct the live camera adapter while internal DMA memory is still
    // contiguous. Sensor initialization remains lazy and is triggered only by
    // LIVE-010 through the video/preview paths.
    static_cast<void>(firmware::target::HardwareAdapterFactory::camera());
    firmware::target::SdStorageAdapter& sd_storage =
        firmware::target::HardwareAdapterFactory::sd_storage();
    static_cast<void>(sd_storage.mount_for_boot());
    sd_storage.start();
    static firmware::target::ConnectivityStartupAdapter connectivity_adapter;
    const std::string machine_name = configured_machine_name();
    const auto access_point_settings = connectivity_adapter.load_settings();
    firmware::target::configure_tcp_discovery_machine_name(machine_name);
    if (!firmware::application::ConnectivityStartup::start(connectivity_adapter,
                                                           machine_name,
                                                           access_point_settings)) {
        ESP_LOGE(tag, "Connectivity startup failed; restarting");
        esp_restart();
    }
    firmware::target::initialize_access_point_commands(machine_name,
                                                        access_point_settings);
    static firmware::target::BlufiProvisioningAdapter blufi_port;
    static firmware::application::StationRuntime blufi_station_runtime;
    static firmware::application::BleProvisioning blufi_provisioning(
        blufi_station_runtime, blufi_port, machine_name);
    static firmware::target::BlufiCallbackAdapter blufi_callbacks(blufi_provisioning);
    static firmware::target::BlufiLifecycleAdapter blufi_lifecycle;
    if (!blufi_lifecycle.start(&blufi_callbacks.callbacks()) ||
        !blufi_provisioning.start()) {
        ESP_LOGW(tag, "BLUFI lifecycle did not start");
    }
    static firmware::target::WebVolumeAdapter web_volume_adapter;
    static firmware::application::WebVolumeStartup web_volume_startup;
    web_volume_startup.start(web_volume_adapter);
    static firmware::target::HttpServerAdapter http_server;
    http_server.start();
    static firmware::target::CanopenTargetService canopen_service;
    canopen_service.start();
    static firmware::target::StorageRetentionAdapter retention_adapter;
    retention_adapter.start();
    static firmware::target::RuntimeCounterTask runtime_counter_task;
    runtime_counter_task.start();
    static firmware::target::ControllerCommandLoop controller_command_loop;
    controller_command_loop.start();
    static firmware::target::RecordingTaskAdapter recording_task;
    recording_task.start();
    static firmware::target::FirmwareUpdateAdapter firmware_update;
    firmware_update.start();
    static firmware::target::TcpControlAdapter tcp_control;
    static firmware::application::StationRuntime automatic_station_runtime;
    static firmware::target::AutomaticConnectionAdapter automatic_connection;
    automatic_connection.start(automatic_station_runtime);
    wlan_events.set_automatic_connection(&automatic_connection);
    wlan_events.set_ble_provisioning(&blufi_provisioning);
    tcp_control.start();
    firmware::target::start_tcp_discovery_task();
    // BOOT-012 requires USB reception to start only after every earlier frame
    // destination has had its startup attempt and retention policy installed.
    static firmware::target::UsbDeviceAdapter usb_device;
    if (!usb_device.start()) {
        ESP_LOGW(tag, "USB device startup failed; USB remains unavailable");
    }
#if defined(Z1_OTA_ROLLBACK_TEST_FAILURE)
    ESP_LOGE(tag, "Rollback HIL fault injected before OTA image validation");
    esp_restart();
    return;
#endif
    const esp_err_t validation_result = esp_ota_mark_app_valid_cancel_rollback();
    if (validation_result == ESP_OK) {
        ESP_LOGI(tag, "OTA image marked valid after critical startup");
    } else if (validation_result != ESP_ERR_OTA_ROLLBACK_INVALID_STATE) {
        ESP_LOGE(tag, "Could not mark OTA image valid: %s",
                 esp_err_to_name(validation_result));
    }
}
