# Builds portable application policy as an ESP-IDF component or host library.
set(APPLICATION_SOURCES
    src/ownership.cpp
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
