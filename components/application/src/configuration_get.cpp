// Implements config-get cache lifetime, fresh SD parsing, and exact replies.
#include "firmware/application/configuration_get.hpp"
#include "firmware/application/configuration_tags.hpp"

#include "firmware/core/configuration_syntax.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_get_tokens = 3U;
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
        const std::string tag = tokens.size() == 3U
                                    ? tokens[1]
                                    : std::string(mainboard_configuration_tag);
        const std::string_view key = tokens.size() == 3U ? tokens[2] : tokens[1];
        send_result(core::protocol::console_message, sd_source, key,
                    port.read_value(tag, key), port);
        return;
    }
    if (tokens[0] == live_source) {
        live.ensure_loaded(port);
        send_result(core::protocol::console_message, live_source, tokens[1],
                    live.find(tokens[1]), port);
    }
}

}  // namespace firmware::application
