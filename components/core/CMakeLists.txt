# Builds portable logic as either an ESP-IDF component or a host-side library.
set(CORE_SOURCES
    src/canopen_dictionary.cpp
    src/canopen_node.cpp
    src/canopen_sdo.cpp
    src/configuration_syntax.cpp
    src/crc.cpp
    src/discovery_policy.cpp
    src/file_transfer_paths.cpp
    src/filesystem_syntax.cpp
    src/frame.cpp
    src/http_policy.cpp
    src/json_input.cpp
    src/multipart_policy.cpp
    src/media_messages.cpp
    src/multipart_extractor.cpp
    src/network_policy.cpp
    src/status.cpp
    src/text.cpp
    src/update_package.cpp
    src/web_static.cpp
    src/web_routes.cpp
)
if(COMMAND idf_component_register)
    idf_component_register(SRCS ${CORE_SOURCES} INCLUDE_DIRS include)
else()
    add_library(firmware_core ${CORE_SOURCES})
    target_include_directories(firmware_core PUBLIC include)
    target_compile_options(firmware_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
