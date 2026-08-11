/** @file @brief Implements exact SoftAP mutation and parameter-query commands. */
#include "application/connectivity/access_point_command.hpp"

#include "core/protocol/protocol_constants.hpp"

#include <cctype>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_ssid_size = 23U;
constexpr std::size_t maximum_retained_access_point_name_size = 31U;
constexpr std::size_t minimum_password_size = 8U;
constexpr std::size_t maximum_password_size = 63U;
constexpr std::uint8_t minimum_saved_channel = 1U;
constexpr std::uint8_t maximum_saved_channel = 11U;

bool ascii_whitespace(char value) {
    const unsigned char byte = static_cast<unsigned char>(value);
    return byte == ' ' || (byte >= '\t' && byte <= '\r');
}

std::string trim_trailing_ascii_whitespace(std::string value) {
    while (!value.empty() && ascii_whitespace(value.back())) {
        value.pop_back();
    }
    return value;
}

std::string decode(std::string_view value) {
    const std::size_t terminator = value.find('\0');
    value = value.substr(0U, terminator);
    return core::decode_escaped(core::BytesView(
        reinterpret_cast<const std::uint8_t*>(value.data()), value.size()));
}

core::Frame response(std::string_view payload) {
    return {core::protocol::console_message,
            core::ByteVector(payload.begin(), payload.end())};
}

std::string selected_name(const AccessPointCommandState& state) {
    if (!state.machine_name.empty()) {
        return state.machine_name.substr(0U, maximum_retained_access_point_name_size);
    }
    if (!state.last_access_point_name.empty()) {
        return state.last_access_point_name.substr(
            0U, maximum_retained_access_point_name_size);
    }
    return state.fallback_name.substr(0U, maximum_retained_access_point_name_size);
}

std::optional<std::uint8_t> initial_channel(std::string_view token) {
    if (token.empty() || token.front() < '0' || token.front() > '9') {
        return std::nullopt;
    }
    std::uint32_t value = 0U;
    for (const char character : token) {
        if (character < '0' || character > '9') {
            break;
        }
        const std::uint32_t digit = static_cast<std::uint32_t>(character - '0');
        if (value > (std::numeric_limits<std::uint32_t>::max() - digit) / 10U) {
            return std::nullopt;
        }
        value = value * 10U + digit;
    }
    if (value < minimum_saved_channel || value > maximum_saved_channel) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(value);
}

struct ParsedApCommand {
    std::string operation;
    std::string remainder;
};

std::optional<ParsedApCommand> parse_ap(core::BytesView payload) {
    if (payload.size() < 3U || payload[0] != 'a' || payload[1] != 'p' ||
        payload[2] != ' ') {
        return std::nullopt;
    }
    std::string input(payload.begin() + 3, payload.end());
    const std::size_t terminator = input.find('\0');
    if (terminator != std::string::npos) {
        input.resize(terminator);
    }
    input = trim_trailing_ascii_whitespace(std::move(input));
    const std::size_t delimiter = input.find(' ');
    ParsedApCommand parsed;
    parsed.operation = input.substr(0U, delimiter);
    if (delimiter != std::string::npos) {
        parsed.remainder = input.substr(delimiter + 1U);
    }
    return parsed;
}

std::string first_literal_space_token(std::string_view input) {
    const std::size_t first = input.find_first_not_of(' ');
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t end = input.find(' ', first);
    return decode(input.substr(first, end - first));
}

bool has_later_literal_space_token(std::string_view input) {
    const std::size_t first = input.find_first_not_of(' ');
    if (first == std::string_view::npos) {
        return false;
    }
    const std::size_t end = input.find(' ', first);
    return end != std::string_view::npos &&
           input.find_first_not_of(' ', end) != std::string_view::npos;
}

std::optional<core::Frame> execute_ap(
    core::BytesView payload, AccessPointCommandState& state,
    AccessPointCommandPort& port) {
    const auto parsed = parse_ap(payload);
    if (!parsed.has_value()) {
        return response("ERROR: Invalid AP Command!\r\n");
    }
    if (parsed->operation == "get") {
        const std::string password = state.password.empty() ? "null" : state.password;
        const unsigned channel = state.saved_channel.value_or(state.last_channel);
        return response("AP enable=" + std::to_string(state.enabled ? 1 : 0) +
                        " ssid=" + selected_name(state) + " password=" + password +
                        " channel=" + std::to_string(channel) + "\r\n");
    }
    if (parsed->operation == "channel") {
        const std::string token = first_literal_space_token(parsed->remainder);
        if (token == "clear" && !has_later_literal_space_token(parsed->remainder)) {
            state.saved_channel.reset();
            return response(port.persist_channel(std::nullopt)
                                ? "AP channel cleared, auto select on reboot\r\n"
                                : "Failed to clear AP channel\r\n");
        }
        const auto channel = initial_channel(token);
        if (!channel.has_value()) {
            return response("WiFi AP Channel should between 1 to 11\r\n");
        }
        state.saved_channel = channel;
        return response(port.persist_channel(channel)
                            ? "AP channel saved, apply on reboot\r\n"
                            : "Failed to save AP channel\r\n");
    }
    if (parsed->operation == "ssid") {
        const std::size_t first = parsed->remainder.find_first_not_of(' ');
        const std::string name = first == std::string::npos
                                     ? std::string{}
                                     : decode(parsed->remainder.substr(first));
        if (name.empty() || name.size() > maximum_ssid_size) {
            return response("WiFi AP SSID length should between 1 to 23\r\n");
        }
        state.machine_name = name;
        return response(port.persist_machine_name(name)
                            ? "AP ssid saved, apply on reboot\r\n"
                            : "Failed to save AP ssid\r\n");
    }
    if (parsed->operation == "password") {
        const std::string password = first_literal_space_token(parsed->remainder);
        if (password == "clear" && !has_later_literal_space_token(parsed->remainder)) {
            state.password.clear();
            return response(port.persist_password({})
                                ? "AP password cleared, open network on reboot\r\n"
                                : "Failed to clear AP password\r\n");
        }
        if (password.size() < minimum_password_size ||
            password.size() > maximum_password_size) {
            return response("WiFi AP password length should between 8 to 63\r\n");
        }
        state.password = password;
        return response(port.persist_password(password)
                            ? "AP password saved, apply on reboot\r\n"
                            : "Failed to save AP password\r\n");
    }
    if (parsed->operation == "enable" || parsed->operation == "disable") {
        const bool enable = parsed->operation == "enable";
        state.enabled = enable;
        if (!port.persist_enabled(enable)) {
            return response(enable ? "Failed to save AP enable\r\n"
                                   : "Failed to save AP disable\r\n");
        }
        if (!enable) {
            return response(port.disable_access_point() ? "AP disabled\r\n"
                                                        : "Failed to disable AP\r\n");
        }
        const std::uint8_t channel = state.saved_channel.value_or(
            state.last_channel >= 1U && state.last_channel <= 11U
                ? state.last_channel
                : 1U);
        AccessPointRadioConfig config{selected_name(state), state.password, channel};
        return response(port.enable_access_point(config) ? "AP enabled\r\n"
                                                         : "Failed to enable AP\r\n");
    }
    return response("ERROR: Invalid AP Command!\r\n");
}

std::optional<core::Frame> execute_query(
    bool station, core::BytesView payload, AccessPointCommandPort& port) {
    std::string input(payload.begin(), payload.end());
    input = trim_trailing_ascii_whitespace(std::move(input));
    std::uint8_t parameter = 0U;
    const bool valid = input.size() == 4U ||
        (input.size() == 6U && input[4] == '.' && input[5] >= '0' && input[5] <= '7');
    if (!valid) {
        return response(station ? "Query WiFi STA parameters ERROR!\n"
                                : "Query WiFi AP parameters ERROR!\n");
    }
    if (input.size() == 6U) {
        parameter = static_cast<std::uint8_t>(input[5] - '0');
    }
    const auto value = station ? port.station_parameter(parameter)
                               : port.access_point_parameter(parameter);
    if (!value.has_value()) {
        return response(station ? "Query WiFi STA parameters ERROR!\n"
                                : "Query WiFi AP parameters ERROR!\n");
    }
    const std::string rendered = value->empty() ? "null" : *value;
    return response(std::string(station ? "M482" : "M483") + " param[" +
                    std::to_string(parameter) + "]:" + rendered + "\n");
}

}  // namespace

std::optional<core::Frame> AccessPointCommandService::execute(
    core::CommandKind kind, core::BytesView payload,
    AccessPointCommandState& state, AccessPointCommandPort& port) {
    if (kind == core::CommandKind::access_point) {
        return execute_ap(payload, state, port);
    }
    if (kind == core::CommandKind::station_parameter_query) {
        return execute_query(true, payload, port);
    }
    if (kind == core::CommandKind::access_point_parameter_query) {
        return execute_query(false, payload, port);
    }
    return std::nullopt;
}

}  // namespace firmware::application
