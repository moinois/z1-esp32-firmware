# The host test executable intentionally has no third-party test dependency.
add_executable(
    core_tests
    test_main.cpp
    test_crc.cpp
    test_controller_query.cpp
    test_controller_link.cpp
    test_controller_firmware_transfer.cpp
    test_controller_config_transfer.cpp
    test_controller_factory_transfer.cpp
    test_controller_snapshots.cpp
    test_controller_transfer.cpp
    test_controller_uart_config.cpp
    test_directory_listing.cpp
    test_frame.cpp
    test_file_transfer_paths.cpp
    test_file_transfer_admission.cpp
    test_file_download.cpp
    test_file_upload.cpp
    test_filesystem_commands.cpp
    test_filesystem_syntax.cpp
    test_play_session.cpp
    test_play_controller.cpp
    test_play_data.cpp
    test_play_line_reader.cpp
    test_play_goto.cpp
    test_routing.cpp
    test_text.cpp
    test_status.cpp
    test_update.cpp
)
target_link_libraries(core_tests PRIVATE firmware_application)
target_compile_options(core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME core_tests COMMAND core_tests)
