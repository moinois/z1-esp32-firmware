/** @file @brief Selects live or simulated hardware adapters from the reviewed Kconfig policy. */
#include "hardware_adapter_factory.hpp"

#include "sdkconfig.h"
#include "esp_log.h"
#include "application/runtime/hardware_adapter_selection.hpp"
#include "sd_card_adapter.hpp"
#include "camera_adapter.hpp"
#include "controller_uart_adapter.hpp"

#ifndef CONFIG_Z1_MOCK_ALL_HARDWARE
#define CONFIG_Z1_MOCK_ALL_HARDWARE 0
#endif

#ifndef CONFIG_Z1_MOCK_SD_HARDWARE
#define CONFIG_Z1_MOCK_SD_HARDWARE 0
#endif

#ifndef CONFIG_Z1_MOCK_CAMERA_HARDWARE
#define CONFIG_Z1_MOCK_CAMERA_HARDWARE 0
#endif

#ifndef CONFIG_Z1_MOCK_CONTROLLER_HARDWARE
#define CONFIG_Z1_MOCK_CONTROLLER_HARDWARE 0
#endif

#ifndef CONFIG_Z1_MOCK_NVS_HARDWARE
#define CONFIG_Z1_MOCK_NVS_HARDWARE 0
#endif

#ifndef CONFIG_Z1_MOCK_NETWORK_HARDWARE
#define CONFIG_Z1_MOCK_NETWORK_HARDWARE 0
#endif

#if CONFIG_Z1_MOCK_SD_HARDWARE || CONFIG_Z1_MOCK_ALL_HARDWARE
#include "mock_sd_card_adapter.hpp"
#endif
#if CONFIG_Z1_MOCK_CAMERA_HARDWARE || CONFIG_Z1_MOCK_ALL_HARDWARE
#include "mock_camera_adapter.hpp"
#endif
#if CONFIG_Z1_MOCK_CONTROLLER_HARDWARE || CONFIG_Z1_MOCK_ALL_HARDWARE
#include "mock_controller_channel_adapter.hpp"
#endif

namespace firmware::target {

namespace {
constexpr char tag[] = "HW_FACTORY";
}

SdStorageAdapter& HardwareAdapterFactory::sd_storage() {
    constexpr firmware::application::HardwareAdapterSelection selection{
        CONFIG_Z1_MOCK_ALL_HARDWARE != 0,
        CONFIG_Z1_MOCK_SD_HARDWARE != 0,
        CONFIG_Z1_MOCK_CAMERA_HARDWARE != 0,
        CONFIG_Z1_MOCK_CONTROLLER_HARDWARE != 0,
        CONFIG_Z1_MOCK_NVS_HARDWARE != 0,
        CONFIG_Z1_MOCK_NETWORK_HARDWARE != 0,
    };
#if CONFIG_Z1_MOCK_SD_HARDWARE || CONFIG_Z1_MOCK_ALL_HARDWARE
    if constexpr (selection.mock_sd()) {
        ESP_LOGW(tag, "SD adapter selected: MOCK");
        static MockSdCardAdapter adapter;
        return adapter;
    }
#endif
    ESP_LOGI(tag, "SD adapter selected: LIVE");
    static SdCardAdapter adapter;
    return adapter;
}

CameraHardwareAdapter& HardwareAdapterFactory::camera() {
    constexpr firmware::application::HardwareAdapterSelection selection{
        CONFIG_Z1_MOCK_ALL_HARDWARE != 0,
        CONFIG_Z1_MOCK_SD_HARDWARE != 0,
        CONFIG_Z1_MOCK_CAMERA_HARDWARE != 0,
        CONFIG_Z1_MOCK_CONTROLLER_HARDWARE != 0,
        CONFIG_Z1_MOCK_NVS_HARDWARE != 0,
        CONFIG_Z1_MOCK_NETWORK_HARDWARE != 0,
    };
#if CONFIG_Z1_MOCK_CAMERA_HARDWARE || CONFIG_Z1_MOCK_ALL_HARDWARE
    if constexpr (selection.mock_camera()) {
        static const bool selection_logged = [] {
            ESP_LOGW(tag, "Camera adapter selected: MOCK");
            return true;
        }();
        static_cast<void>(selection_logged);
        static MockCameraAdapter adapter;
        return adapter;
    }
#endif
    static const bool selection_logged = [] {
        ESP_LOGI(tag, "Camera adapter selected: LIVE");
        return true;
    }();
    static_cast<void>(selection_logged);
    static CameraAdapter adapter;
    return adapter;
}

ControllerChannelAdapter& HardwareAdapterFactory::controller_channel() {
    constexpr firmware::application::HardwareAdapterSelection selection{
        CONFIG_Z1_MOCK_ALL_HARDWARE != 0,
        CONFIG_Z1_MOCK_SD_HARDWARE != 0,
        CONFIG_Z1_MOCK_CAMERA_HARDWARE != 0,
        CONFIG_Z1_MOCK_CONTROLLER_HARDWARE != 0,
        CONFIG_Z1_MOCK_NVS_HARDWARE != 0,
        CONFIG_Z1_MOCK_NETWORK_HARDWARE != 0,
    };
#if CONFIG_Z1_MOCK_CONTROLLER_HARDWARE || CONFIG_Z1_MOCK_ALL_HARDWARE
    if constexpr (selection.mock_controller()) {
        static const bool selection_logged = [] {
            ESP_LOGW(tag, "Controller adapter selected: MOCK");
            return true;
        }();
        static_cast<void>(selection_logged);
        static MockControllerChannelAdapter adapter;
        return adapter;
    }
#endif
    static const bool selection_logged = [] {
        ESP_LOGI(tag, "Controller adapter selected: LIVE");
        return true;
    }();
    static_cast<void>(selection_logged);
    static ControllerUartAdapter adapter;
    return adapter;
}

bool HardwareAdapterFactory::nvs_faults_enabled() {
    constexpr firmware::application::HardwareAdapterSelection selection{
        CONFIG_Z1_MOCK_ALL_HARDWARE != 0,
        CONFIG_Z1_MOCK_SD_HARDWARE != 0,
        CONFIG_Z1_MOCK_CAMERA_HARDWARE != 0,
        CONFIG_Z1_MOCK_CONTROLLER_HARDWARE != 0,
        CONFIG_Z1_MOCK_NVS_HARDWARE != 0,
        CONFIG_Z1_MOCK_NETWORK_HARDWARE != 0,
    };
    return selection.mock_nvs();
}

bool HardwareAdapterFactory::network_faults_enabled() {
    constexpr firmware::application::HardwareAdapterSelection selection{
        CONFIG_Z1_MOCK_ALL_HARDWARE != 0,
        CONFIG_Z1_MOCK_SD_HARDWARE != 0,
        CONFIG_Z1_MOCK_CAMERA_HARDWARE != 0,
        CONFIG_Z1_MOCK_CONTROLLER_HARDWARE != 0,
        CONFIG_Z1_MOCK_NVS_HARDWARE != 0,
        CONFIG_Z1_MOCK_NETWORK_HARDWARE != 0,
    };
    return selection.mock_network();
}

}  // namespace firmware::target
