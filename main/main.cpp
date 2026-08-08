// Starts the mainboard services in the normative order defined by BOOT-010 through BOOT-019.
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

#include "firmware/application/web_volume_startup.hpp"
#include "firmware/application/connectivity_startup.hpp"
#include "firmware/core/network_policy.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace {
constexpr std::uint32_t heartbeat_stack_size = 2048U;
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

// Initializes NVS with the erase-and-retry recovery required during early boot.
bool initialize_persistent_store() {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(tag, "NVS分区需要擦除，正在擦除...");
        result = nvs_flash_erase();
        if (result == ESP_OK) {
            result = nvs_flash_init();
        }
    } else if (result != ESP_OK) {
        ESP_LOGE(tag, "NVS初始化失败: %s (0x%x)", esp_err_to_name(result), static_cast<unsigned>(result));
        ESP_LOGW(tag, "尝试擦除NVS分区并重新初始化...");
        result = nvs_flash_erase();
        if (result == ESP_OK) {
            result = nvs_flash_init();
        }
    }
    if (result != ESP_OK) {
        ESP_LOGE(tag, "NVS初始化仍然失败，系统无法继续运行");
        return false;
    }
    ESP_LOGI(tag, "NVS初始化成功");
    return true;
}

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
    if (!initialize_persistent_store()) {
        ESP_LOGE(tag, "Restarting because NVS initialization failed");
        esp_restart();
    }
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
    firmware::target::SdStorageAdapter& sd_storage =
        firmware::target::HardwareAdapterFactory::sd_storage();
    static_cast<void>(sd_storage.mount_for_boot());
    sd_storage.start();
    static firmware::target::ConnectivityStartupAdapter connectivity_adapter;
    const std::string machine_name = configured_machine_name();
    firmware::target::configure_tcp_discovery_machine_name(machine_name);
    if (!firmware::application::ConnectivityStartup::start(connectivity_adapter,
                                                           machine_name)) {
        ESP_LOGE(tag, "Connectivity startup failed; restarting");
        esp_restart();
    }
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
    if (!firmware::target::HardwareAdapterFactory::camera().initialize()) {
        ESP_LOGW(tag, "Camera startup failed; camera endpoints remain unavailable");
    }
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
