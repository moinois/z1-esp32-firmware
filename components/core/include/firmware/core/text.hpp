/** @file @brief Shared command recognition, escape decoding, and path cleanup. */
#pragma once
#include "firmware/core/bytes.hpp"
#include <cstddef>
#include <string>
namespace firmware::core {
/** Decodes the protocol's backslash/escape representation without tokenizing. */
std::string decode_escaped(BytesView input);
/** Lexically normalizes separators and dot components without filesystem I/O. */
std::string normalize_path(std::string input);

/** Semantic command selected by the shared, transport-independent classifier. */
enum class CommandKind {
    unknown,
    status,
    list,
    make_directory,
    remove,
    move,
    file_type,
    md5_sum,
    config_restore,
    config_default,
    config_get,
    config_set,
    time,
    wlan,
    can_exercise,
    record_start,
    record_stop,
    serial_get,
    serial_set,
    system_time,
    clear_first_time,
    upgrade,
    reset,
    diagnose,
    wifi_diagnose,
    mock_sd_control,
    mock_nvs_control,
    mock_network_control,
    version
};

/** Result of recognizing the leading token in a framed text command. */
struct CommandMatch {
    /// Semantic command, or `unknown` when no supported token matched.
    CommandKind kind = CommandKind::unknown;
    /// First byte after the recognized command and its delimiter.
    std::size_t argument_offset = 0;
    /// Distinguishes a recognized command from an unknown or malformed prefix.
    bool accepted = false;
};

/** Recognizes a bounded command prefix without decoding its arguments.
 *  @param payload Complete text-command payload.
 *  @return Command identity and argument slice offset for downstream parsers.
 */
CommandMatch recognize_command(BytesView payload);
}  // namespace firmware::core
