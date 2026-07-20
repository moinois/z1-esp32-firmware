# Builds portable application policy as an ESP-IDF component or host library.
set(APPLICATION_SOURCES
    src/controller_config_transfer.cpp
    src/controller_factory_transfer.cpp
    src/controller_firmware_transfer.cpp
    src/controller_link.cpp
    src/controller_query.cpp
    src/controller_snapshots.cpp
    src/controller_transfer.cpp
    src/ownership.cpp
    src/play_controller.cpp
    src/play_session.cpp
    src/router.cpp
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
