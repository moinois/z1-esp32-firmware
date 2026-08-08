/** @file @brief Implements MD5 path validation, lowercase formatting, and exact errors. */
#include "firmware/application/file_hash_command.hpp"

#include "firmware/core/filesystem_syntax.hpp"
#include "firmware/core/file_transfer_limits.hpp"
#include "firmware/core/protocol_constants.hpp"
#include "firmware/core/sd_user_path.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t hash_read_block_size = 4096U;
constexpr std::string_view missing_argument_message =
    "Error: md5sum requires a file path\r\n";

// Sends one console response built from owned text.
void send_console(std::string message, FileHashPort& port) {
    port.send({core::protocol::console_message,
               {message.begin(), message.end()}});
}

// Validates exactly one MD5 value and converts its hexadecimal text to lowercase.
std::optional<std::string> normalize_md5(std::string value) {
    if (value.size() != core::file_transfer_limits::md5_text_size) {
        return std::nullopt;
    }
    const bool hexadecimal = std::all_of(
        value.begin(), value.end(), [](unsigned char character) {
            return std::isxdigit(character) != 0;
        });
    if (!hexadecimal) {
        return std::nullopt;
    }
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

}  // namespace

void FileHashCommand::execute(core::BytesView argument, FileHashPort& port) {
    const auto path = core::parse_filesystem_path(argument);
    if (!path.has_value()) {
        send_console(std::string(missing_argument_message), port);
        return;
    }
    const std::string displayed = core::logical_sd_path(*path);

    switch (port.inspect_path(*path)) {
        case FileHashPathState::resolution_failure:
            return;
        case FileHashPathState::missing:
            send_console("Error: file not found [" + displayed + "]\r\n", port);
            return;
        case FileHashPathState::not_regular:
            send_console("Error: not a file [" + displayed + "]\r\n", port);
            return;
        case FileHashPathState::regular_file:
            break;
    }

    const auto calculated = port.calculate_md5(*path, hash_read_block_size);
    const auto normalized = calculated.has_value()
                                ? normalize_md5(*calculated)
                                : std::optional<std::string>{};
    if (!normalized.has_value()) {
        send_console("Error: md5sum failed [" + displayed + "]\r\n", port);
        return;
    }
    send_console(*normalized + displayed + "\r\n", port);
}

}  // namespace firmware::application
