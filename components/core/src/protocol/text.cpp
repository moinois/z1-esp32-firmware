/** @file @brief Implements shared byte escaping, normalized paths, and ordered prefix matching. */
#include "core/protocol/text.hpp"
#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace firmware::core {
namespace {

constexpr std::uint8_t first_escape_code = 1U;
constexpr std::uint8_t last_escape_code = 5U;
constexpr std::size_t maximum_bounded_command_size = 128U;

bool ascii_whitespace(std::uint8_t byte) {
    return byte == ' ' || (byte >= '\t' && byte <= '\r');
}

}  // namespace

std::string decode_escaped(BytesView input) {
    constexpr std::array<char, 5> replacement{' ', '?', '*', '!', '~'};
    std::string output;
    for (const auto byte : input) {
        if (byte == 0U) {
            break;
        }
        const bool escaped = byte >= first_escape_code && byte <= last_escape_code;
        output.push_back(escaped ? replacement[byte - first_escape_code]
                                 : static_cast<char>(byte));
    }
    return output;
}
std::string normalize_path(std::string input) {
    std::replace(input.begin(), input.end(), '\\', '/');
    std::vector<std::string_view> parts;
    for (std::size_t start = 0; start <= input.size();) {
        const auto end = input.find('/', start);
        const std::string_view part(input.data() + start, (end == std::string::npos ? input.size() : end) - start);
        if (!part.empty() && part != ".") {
            if (part == "..") {
                if (!parts.empty()) {
                    parts.pop_back();
                }
            } else {
                parts.push_back(part);
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1U;
    }
    std::string output = "/";
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (index != 0) {
            output += '/';
        }
        output.append(parts[index]);
    }
    return output;
}
CommandMatch recognize_command(BytesView payload) {
    struct Rule {
        std::string_view prefix;
        CommandKind kind;
        std::size_t offset;
        bool unlimited;
    };
    if (payload.size() >= 3U && payload[0] == 'a' && payload[1] == 'p' &&
        (payload[2] == 0U || ascii_whitespace(payload[2]))) {
        const std::size_t offset =
            payload[2] == ' ' ? 3U : payload.size();
        return {CommandKind::access_point, offset,
                payload.size() <= maximum_bounded_command_size};
    }

    static constexpr Rule rules[] = {
        {"?", CommandKind::status, 0, true},
        {"ls", CommandKind::list, 2, false},
        {"mkdir", CommandKind::make_directory, 5, false},
        {"rm", CommandKind::remove, 2, false},
        {"mv", CommandKind::move, 2, false},
        {"ftype", CommandKind::file_type, 0, true},
        {"md5sum", CommandKind::md5_sum, 7, false},
        {"config-restore", CommandKind::config_restore, 15, false},
        {"config-default", CommandKind::config_default, 15, false},
        {"config-get", CommandKind::config_get, 11, false},
        {"config-set", CommandKind::config_set, 11, false},
        {"time", CommandKind::time, 4, false},
        {"M482", CommandKind::station_parameter_query, 0, false},
        {"M483", CommandKind::access_point_parameter_query, 0, false},
        {"wlan", CommandKind::wlan, 0, false},
        {"M942", CommandKind::can_exercise, 0, false},
        {"M951", CommandKind::record_start, 0, true},
        {"M952", CommandKind::record_stop, 0, true},
        {"sn-get", CommandKind::serial_get, 0, false},
        {"sn-set", CommandKind::serial_set, 0, false},
        {"sys-time", CommandKind::system_time, 0, false},
        {"clearftm", CommandKind::clear_first_time, 0, false},
        {"upgrade", CommandKind::upgrade, 0, true},
        {"reset", CommandKind::reset, 0, true},
        {"diagnose", CommandKind::diagnose, 0, true},
        {"wifi-diag", CommandKind::wifi_diagnose, 0, true},
        {"mock-sd", CommandKind::mock_sd_control, 7, false},
        {"mock-nvs", CommandKind::mock_nvs_control, 8, false},
        {"mock-net", CommandKind::mock_network_control, 8, false},
        {"version", CommandKind::version, 0, true},
    };
    for (const auto& rule : rules) {
        if (payload.size() >= rule.prefix.size() &&
            std::equal(rule.prefix.begin(), rule.prefix.end(), payload.begin())) {
            return {rule.kind, payload.size() > rule.offset ? rule.offset : payload.size(),
                    rule.unlimited || payload.size() <= maximum_bounded_command_size};
        }
    }
    return {};
}
}  // namespace firmware::core
