// Declares shared command text, escaping, and safe path operations.
#pragma once
#include "firmware/core/bytes.hpp"
#include <cstddef>
#include <string>
namespace firmware::core {
std::string decode_escaped(BytesView input);
std::string normalize_path(std::string input);
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
    version
};

struct CommandMatch {
    CommandKind kind = CommandKind::unknown;
    std::size_t argument_offset = 0;
    bool accepted = false;
};
CommandMatch recognize_command(BytesView payload);
}  // namespace firmware::core
