# Builds portable logic as either an ESP-IDF component or a host-side library.
set(CORE_SOURCES
    src/can/can_error_policy.cpp
    src/can/canopen_dictionary.cpp
    src/can/canopen_pdo.cpp
    src/can/canopen_tpdo.cpp
    src/media/avi_preview.cpp
    src/media/avi_writer.cpp
    src/can/canopen_node.cpp
    src/can/canopen_sdo.cpp
    src/can/canopen_sdo_client.cpp
    src/can/canopen_sdo_mailbox.cpp
    src/configuration/configuration_syntax.cpp
    src/protocol/crc.cpp
    src/network/discovery_policy.cpp
    src/filesystem/file_transfer_paths.cpp
    src/filesystem/filesystem_syntax.cpp
    src/protocol/frame.cpp
    src/web/http_policy.cpp
    src/configuration/json_input.cpp
    src/web/multipart_policy.cpp
    src/media/media_messages.cpp
    src/web/multipart_extractor.cpp
    src/network/network_policy.cpp
    src/media/preview_path_policy.cpp
    src/protocol/status.cpp
    src/protocol/text.cpp
    src/update/update_package.cpp
    src/web/web_static.cpp
    src/web/web_routes.cpp
    src/connectivity/wifi_statistics.cpp
)
if(COMMAND idf_component_register)
    idf_component_register(SRCS ${CORE_SOURCES} INCLUDE_DIRS include)
else()
    add_library(firmware_core ${CORE_SOURCES})
    target_include_directories(firmware_core PUBLIC include)
    target_compile_options(firmware_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
