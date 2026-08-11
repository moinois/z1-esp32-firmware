/** @file @brief Implements config-get cache lifetime, fresh SD parsing, and exact replies. */
#include "application/configuration/configuration_get.hpp"
#include "application/configuration/configuration_sources.hpp"
#include "application/configuration/configuration_tags.hpp"

#include "core/configuration/configuration_syntax.hpp"
#include "core/protocol/protocol_constants.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_get_tokens = 3U;
constexpr std::size_t missing_result_limit = 255U;
constexpr std::size_t found_result_limit = 511U;

bool selector_matches(std::string_view supplied, std::string_view expected) {
    return core::configuration_hash(supplied) == core::configuration_hash(expected);
}

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
    message.resize(std::min(message.size(), value.has_value()
                                               ? found_result_limit
                                               : missing_result_limit));
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
        send_result(core::protocol::text_response, configuration_sources::cached, tokens[0],
                    value, port);
        live.reset();
        return;
    }

    if (selector_matches(tokens[0], configuration_sources::sd)) {
        const std::string tag = tokens.size() == 3U
                                    ? tokens[1]
                                    : std::string(mainboard_configuration_tag);
        const std::string_view key = tokens.size() == 3U ? tokens[2] : tokens[1];
        send_result(core::protocol::console_message, tokens[0], key,
                    port.read_value(tag, key), port);
        return;
    }
    if (selector_matches(tokens[0], configuration_sources::live)) {
        live.ensure_loaded(port);
        send_result(core::protocol::console_message, tokens[0], tokens[1],
                    live.find(tokens[1]), port);
    }
}

}  // namespace firmware::application
