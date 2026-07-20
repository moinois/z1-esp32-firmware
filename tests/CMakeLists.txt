# The host test executable intentionally has no third-party test dependency.
add_executable(core_tests
    test_main.cpp
    test_crc.cpp
    test_controller_query.cpp
    test_controller_link.cpp
    test_controller_snapshots.cpp
    test_frame.cpp
    test_routing.cpp
    test_text.cpp
    test_status.cpp
    test_update.cpp
)
target_link_libraries(core_tests PRIVATE firmware_application)
target_compile_options(core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME core_tests COMMAND core_tests)
