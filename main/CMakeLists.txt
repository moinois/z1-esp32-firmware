# Composes ESP-IDF adapters around the portable firmware core.
idf_component_register(
    SRCS
        "can/can_twai_adapter.cpp"
        "can/canopen_target_service.cpp"
        "controller/controller_uart_adapter.cpp"
        "main.cpp"
        "hardware_adapter_factory.cpp"
        "configuration/configuration_file_store.cpp"
        "media/http_server_adapter.cpp"
        "media/camera_adapter.cpp"
        "media/camera_settings_adapter.cpp"
        "media/recording_file_adapter.cpp"
        "media/recording_task_adapter.cpp"
        "media/web_volume_adapter.cpp"
        "media/storage_retention_adapter.cpp"
        "storage/sd_card_adapter.cpp"
        "storage/sd_access_diagnostics.cpp"
        "storage/posix_file.cpp"
        "diagnostics/diagnostic_capture_adapter.cpp"
        "diagnostics/serial_log_mirror_adapter.cpp"
        "runtime/nvs_key_value_adapter.cpp"
        "runtime/nvs_command_ports.cpp"
        "runtime/nvs_runtime_counter_adapter.cpp"
        "runtime/runtime_counter_task.cpp"
        "runtime/runtime_operation_capacity.cpp"
        "runtime/heartbeat_adapter.cpp"
        "media/runtime_play_observer.cpp"
        "runtime/wall_clock_adapter.cpp"
        "runtime/wall_clock_command_dispatcher.cpp"
        "controller/controller_command_loop.cpp"
        "controller/controller_transfer_adapter.cpp"
        "controller/controller_play_adapter.cpp"
        "media/play_runtime_state.cpp"
        "media/tcp_play_adapter.cpp"
        "configuration/tcp_configuration_file_adapter.cpp"
        "configuration/tcp_configuration_adapter.cpp"
        "runtime/runtime_status_adapter.cpp"
        "runtime/serial_number_adapter.cpp"
        "transport/tcp_serial_number_adapter.cpp"
        "transport/tcp_runtime_command_adapter.cpp"
        "controller/controller_runtime_command_adapter.cpp"
        "storage/tcp_filesystem_adapter.cpp"
        "storage/tcp_filesystem_query_adapter.cpp"
        "storage/tcp_file_transfer_adapter.cpp"
        "connectivity/tcp_wlan_scan_adapter.cpp"
        "connectivity/esp_wifi_scanner.cpp"
        "connectivity/tcp_wlan_station_adapter.cpp"
        "connectivity/tcp_wlan_connection_adapter.cpp"
        "connectivity/tcp_discovery_adapter.cpp"
        "connectivity/wlan_event_adapter.cpp"
        "connectivity/wifi_diagnostic_log.cpp"
        "connectivity/connectivity_startup_adapter.cpp"
        "connectivity/automatic_connection_adapter.cpp"
        "provisioning/blufi_lifecycle_adapter.cpp"
        "provisioning/blufi_crypto_adapter.cpp"
        "provisioning/blufi_provisioning_adapter.cpp"
        "provisioning/blufi_callback_adapter.cpp"
        "update/esp_image_validator.cpp"
        "update/ota_update_adapter.cpp"
        "update/firmware_update_adapter.cpp"
        "transport/tcp_control_adapter.cpp"
        "media/recording_request_state.cpp"
        "transport/usb_device_adapter.cpp"
    REQUIRES
        application
        core
        driver
        esp_driver_gpio
        esp_driver_uart
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        spiffs
        esp_http_server
        json
        fatfs
        sdmmc
        esp_driver_sdmmc
        log
        esp_timer
        lwip
        bt
        mbedtls
        bootloader_support
        app_update
        esp32-camera
        esp_tinyusb
    PRIV_INCLUDE_DIRS
        "."
        "can"
        "configuration"
        "connectivity"
        "controller"
        "diagnostics"
        "media"
        "provisioning"
        "runtime"
        "storage"
        "transport"
        "update"
        "$ENV{IDF_PATH}/components/bt/common/api/include/api"
        "$ENV{IDF_PATH}/components/bt/common/btc/profile/esp/blufi/include"
        "$ENV{IDF_PATH}/components/bt/host/bluedroid/api/include/api"
        "$ENV{IDF_PATH}/components/bt/include/esp32c3/include"
)

if(CONFIG_Z1_MOCK_ALL_HARDWARE OR CONFIG_Z1_MOCK_SD_HARDWARE OR
   CONFIG_Z1_MOCK_NVS_HARDWARE OR CONFIG_Z1_MOCK_NETWORK_HARDWARE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE Z1_MOCK_CONTROL_ENABLED=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE Z1_MOCK_CONTROL_ENABLED=0)
endif()
if(CONFIG_Z1_MOCK_NETWORK_HARDWARE OR CONFIG_Z1_MOCK_ALL_HARDWARE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE Z1_MOCK_NETWORK_CONTROL_ENABLED=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE Z1_MOCK_NETWORK_CONTROL_ENABLED=0)
endif()

if(CONFIG_Z1_MOCK_NVS_HARDWARE OR CONFIG_Z1_MOCK_ALL_HARDWARE)
    target_sources(${COMPONENT_LIB} PRIVATE "runtime/mock_nvs_fault_adapter.cpp")
endif()
if(CONFIG_Z1_MOCK_CONTROLLER_HARDWARE OR CONFIG_Z1_MOCK_ALL_HARDWARE)
    target_sources(${COMPONENT_LIB} PRIVATE "controller/mock_controller_channel_adapter.cpp")
endif()
if(CONFIG_Z1_MOCK_CAMERA_HARDWARE OR CONFIG_Z1_MOCK_ALL_HARDWARE)
    target_sources(${COMPONENT_LIB} PRIVATE "media/mock_camera_adapter.cpp")
endif()
if(CONFIG_Z1_MOCK_SD_HARDWARE OR CONFIG_Z1_MOCK_ALL_HARDWARE)
    target_sources(${COMPONENT_LIB} PRIVATE "storage/mock_sd_card_adapter.cpp")
endif()
if(CONFIG_Z1_MOCK_NETWORK_HARDWARE OR CONFIG_Z1_MOCK_ALL_HARDWARE)
    target_sources(${COMPONENT_LIB} PRIVATE "connectivity/mock_network_fault_adapter.cpp")
endif()

# Used only by the destructive HIL rollback test. A normal build never defines
# this and marks the new OTA image valid after critical startup completes.
if(DEFINED ENV{Z1_OTA_ROLLBACK_TEST_FAILURE})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        Z1_OTA_ROLLBACK_TEST_FAILURE=1)
endif()
