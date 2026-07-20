// Implements WLAN command tokenization without any target or transport dependency.
#include "firmware/application/wlan_request.hpp"

#include "firmware/core/text.hpp"

#include <cctype>
#include <cstdint>
#include <vector>

namespace firmware::application {
namespace {

std::vector<std::string> tokens(std::string_view text) {
    std::vector<std::string> result;
    std::size_t index = 0U;
    while (index < text.size()) {
        while (index < text.size() && std::isspace(
            static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
        const std::size_t begin = index;
        while (index < text.size() && std::isspace(
            static_cast<unsigned char>(text[index])) == 0) {
            ++index;
        }
        if (begin != index) {
            result.push_back(firmware::core::decode_escaped(
                {reinterpret_cast<const std::uint8_t*>(text.data() + begin),
                 index - begin}));
        }
    }
    return result;
}

}  // namespace

WlanRequest parse_wlan_request(std::string_view command) {
    if (command.size() < 5U || command.substr(0U, 4U) != "wlan"
        || command[4] != ' ') {
        return {};
    }
    bool disconnect = false;
    std::vector<std::string> arguments;
    for (const std::string& token : tokens(command.substr(5U))) {
        if (token == "-d") {
            disconnect = true;
        } else if (token != "-e") {
            arguments.push_back(token);
        }
    }
    if (disconnect) {
        return {WlanRequestKind::disconnect, {}, {}};
    }
    if (arguments.size() >= 2U) {
        return {WlanRequestKind::connect, arguments[0], arguments[1]};
    }
    return {};
}

}  // namespace firmware::application
