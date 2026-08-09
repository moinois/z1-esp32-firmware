/** @file @brief Implements bytewise configuration copies and exact command result mapping. */
#include "application/configuration/configuration_files.hpp"

#include "core/protocol/protocol_constants.hpp"

#include <string>

namespace firmware::application {
namespace {

constexpr std::string_view missing_default_message =
    "Default file not found: /config.default\r\n";
constexpr std::string_view missing_active_message =
    "Config file not found: /config.txt\r\n";
constexpr std::string_view restore_failure_message =
    "Config file not found or created fail: /config.txt\r\n";
constexpr std::string_view save_failure_message =
    "Default file not found or created fail: /config.default\r\n";
constexpr std::string_view restore_success_message =
    "Settings restored complete.\r\n";
constexpr std::string_view save_success_message =
    "Settings save as default complete.\r\n";

// Sends one console response from fixed command text.
void send_console(std::string_view message, ConfigurationFilePort& port) {
    port.send({core::protocol::console_message,
               {message.begin(), message.end()}});
}

// Copies one source byte at a time and retains any partial destination on failure.
bool copy_file(std::string_view source, std::string_view destination,
               ConfigurationFilePort& port) {
    if (!port.open_source(source)) {
        return false;
    }
    if (!port.open_truncated_destination(destination)) {
        port.close_source();
        return false;
    }

    bool succeeded = true;
    for (;;) {
        const ByteRead read = port.read_byte();
        if (read.status == ByteReadStatus::end_of_file) {
            break;
        }
        if (read.status == ByteReadStatus::failure ||
            !port.write_byte(read.value)) {
            succeeded = false;
            break;
        }
    }
    port.close_source();
    if (!port.close_destination()) {
        succeeded = false;
    }
    return succeeded;
}

// Applies source-presence and copy-result messages for one fixed-path operation.
void execute_copy(std::string_view source, std::string_view destination,
                  std::string_view missing_message,
                  std::string_view failure_message,
                  std::string_view success_message,
                  ConfigurationFilePort& port) {
    if (!port.file_exists(source)) {
        send_console(missing_message, port);
        return;
    }
    const bool copied = copy_file(source, destination, port);
    send_console(copied ? success_message : failure_message, port);
}

}  // namespace

void ConfigurationFiles::restore(ConfigurationFilePort& port) {
    execute_copy(port.default_configuration_path(),
                 port.active_configuration_path(),
                 missing_default_message, restore_failure_message,
                 restore_success_message, port);
}

void ConfigurationFiles::save_default(ConfigurationFilePort& port) {
    execute_copy(port.active_configuration_path(),
                 port.default_configuration_path(),
                 missing_active_message, save_failure_message,
                 save_success_message, port);
}

}  // namespace firmware::application
