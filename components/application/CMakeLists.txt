# Builds portable application policy as an ESP-IDF component or host library.
set(APPLICATION_SOURCES
    src/automatic_connection.cpp
    src/ble_provisioning.cpp
    src/blufi_fragment.cpp
    src/blufi_gatt.cpp
    src/blufi_product.cpp
    src/blufi_security.cpp
    src/blufi_session.cpp
    src/blufi_wire.cpp
    src/configuration_files.cpp
    src/configuration_get.cpp
    src/configuration_set.cpp
    src/connectivity_startup.cpp
    src/directory_listing.cpp
    src/diagnostic_capture.cpp
    src/diagnostic_log_writer.cpp
    src/discovery_service.cpp
    src/file_transfer_admission.cpp
    src/file_download.cpp
    src/file_hash_command.cpp
    src/file_upload.cpp
    src/filesystem_commands.cpp
    src/live_configuration.cpp
    src/m942_exercise.cpp
    src/controller_config_transfer.cpp
    src/controller_factory_transfer.cpp
    src/controller_firmware_transfer.cpp
    src/controller_link.cpp
    src/controller_query.cpp
    src/controller_snapshots.cpp
    src/controller_transfer.cpp
    src/ownership.cpp
    src/play_controller.cpp
    src/play_line_reader.cpp
    src/play_session.cpp
    src/router.cpp
    src/runtime_commands.cpp
    src/runtime_counters.cpp
    src/runtime_status.cpp
    src/sd_card_lifecycle.cpp
    src/serial_number.cpp
    src/station_connection.cpp
    src/update_application.cpp
    src/update_controller.cpp
    src/update_deletion.cpp
    src/update_phase.cpp
    src/update_trigger.cpp
    src/update_validation.cpp
    src/wall_clock.cpp
    src/wlan_command.cpp
)

if(COMMAND idf_component_register)
    idf_component_register(
        SRCS ${APPLICATION_SOURCES}
        INCLUDE_DIRS include
        REQUIRES core
    )
else()
    add_library(firmware_application ${APPLICATION_SOURCES})
    target_include_directories(firmware_application PUBLIC include)
    target_link_libraries(firmware_application PUBLIC firmware_core)
    target_compile_options(firmware_application PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
