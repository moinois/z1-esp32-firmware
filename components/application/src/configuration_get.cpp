// Implements config-get cache lifetime, fresh SD parsing, and exact replies.
#include "firmware/application/configuration_get.hpp"

#include "firmware/core/configuration_syntax.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::string_view active_configuration_path = "/sd/config.txt";
constexpr std::size_t maximum_get_tokens = 2U;
constexpr std::string_view cached_source = "cached";
constexpr std::string_view sd_source = "sd";
constexpr std::string_view live_source = "live";

// Sends one source-labelled lookup result with the selected packet type.
void send_result(std::uint8_t packet_type, std::string_view source,
                 std::string_view key,
                 const std::optional<std::string>& value,
                 ConfigurationGetPort& port) {
    std::string message(source);
    message += ": ";
    message += key;
    if (value.has_value()) {
        message += " is set to ";
        message += *value;
    } else {
        message += " is not in config";
    }
    message += "\r\n";
    port.send({packet_type, {message.begin(), message.end()}});
}

// Loads and searches SD configuration lines using fresh file semantics.
std::optional<std::string> find_sd_value(std::string_view key,
                                         ConfigurationGetPort& port) {
    const auto lines = port.read_sd_lines(active_configuration_path);
    if (!lines.has_value()) {
        return std::nullopt;
    }
    for (const std::string& line : *lines) {
        const auto entry = core::parse_sd_config_line(line);
        if (entry.has_value() && entry->key == key) {
            return entry->value;
        }
    }
    return std::nullopt;
}

}  // namespace

void ConfigurationGet::execute(core::BytesView argument,
                               LiveConfiguration& live,
                               ConfigurationGetPort& port) {
    const std::vector<std::string> tokens =
        core::parse_configuration_tokens(argument, maximum_get_tokens);
    if (tokens.empty()) {
        return;
    }

    if (tokens.size() == 1U) {
        live.reset();
        live.ensure_loaded(port);
        const auto value = live.find(tokens[0]);
        send_result(core::protocol::text_response, cached_source, tokens[0],
                    value, port);
        live.reset();
        return;
    }

    if (tokens[0] == sd_source) {
        send_result(core::protocol::console_message, sd_source, tokens[1],
                    find_sd_value(tokens[1], port), port);
        return;
    }
    if (tokens[0] == live_source) {
        live.ensure_loaded(port);
        send_result(core::protocol::console_message, live_source, tokens[1],
                    live.find(tokens[1]), port);
    }
}

}  // namespace firmware::application
