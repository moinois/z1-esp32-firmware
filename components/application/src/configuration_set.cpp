// Implements config-set source selection, atomic replacement steps, and replies.
#include "firmware/application/configuration_set.hpp"

#include "firmware/core/configuration_syntax.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <string>
#include <vector>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_set_tokens = 4U;
constexpr std::string_view sd_source = "sd";
constexpr std::string_view live_source = "live";
constexpr std::string_view usage_message =
    "Usage: config-set source setting value # where source is sd, setting is the key and value is the new value\r\n";

// Sends one console response from owned text.
void send_console(std::string message, ConfigurationSetPort& port) {
    port.send({core::protocol::console_message,
               {message.begin(), message.end()}});
}

// Produces the exact source-labelled success or capacity failure response.
void send_result(std::string_view source, std::string_view key,
                 std::string_view value, bool success,
                 ConfigurationSetPort& port) {
    std::string message(source);
    message += ": ";
    message += key;
    if (success) {
        message += " has been set to ";
        message += value;
    } else {
        message += " not enough space to overwrite existing key/value";
    }
    message += "\r\n";
    send_console(std::move(message), port);
}

// Stores one setting through the replaceable configuration store.
bool set_sd(std::string_view tag, std::string_view key, std::string_view value,
            ConfigurationSetPort& port) {
    return port.set_value(tag, key, value);
}

}  // namespace

void ConfigurationSet::execute(core::BytesView argument,
                               LiveConfiguration& live,
                               ConfigurationSetPort& port) {
    const std::vector<std::string> tokens =
        core::parse_configuration_tokens(argument, maximum_set_tokens);
    if (tokens.size() < 3U || tokens[0].empty() || tokens[1].empty() ||
        tokens[2].empty()) {
        send_console(std::string(usage_message), port);
        return;
    }

    if (tokens[0] == live_source) {
        live.ensure_loaded(port);
        send_result(live_source, tokens[1], tokens[2],
                    live.set(tokens[1], tokens[2]), port);
        return;
    }
    if (tokens[0] == sd_source) {
        const std::string tag = tokens.size() == 4U ? tokens[1] : std::string{};
        const std::string_view key = tokens.size() == 4U ? tokens[2] : tokens[1];
        const std::string_view value = tokens.size() == 4U ? tokens[3] : tokens[2];
        send_result(sd_source, key, value, set_sd(tag, key, value, port), port);
        return;
    }
    send_console(tokens[0] + " source does not exist\r\n", port);
}

}  // namespace firmware::application
