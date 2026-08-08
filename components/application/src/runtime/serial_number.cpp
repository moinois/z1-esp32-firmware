/** @file @brief Implements serial-number syntax, immutable persistence, and exact responses. */
#include "firmware/application/serial_number.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::string_view serial_namespace = "factory";
constexpr std::string_view serial_key = "machine_sn";
constexpr std::string_view get_command = "sn-get";
constexpr std::string_view set_command = "sn-set";
constexpr std::size_t minimum_serial_length = 1U;
constexpr std::size_t maximum_serial_length = 32U;
constexpr std::uint8_t first_printable_ascii = 0x20U;
constexpr std::uint8_t last_printable_ascii = 0x7EU;
constexpr std::uint32_t operation_wait_milliseconds = 200U;
constexpr std::uint8_t runtime_response_type = 0x83U;

// Reports whether one byte is ASCII whitespace accepted around commands.
bool ascii_whitespace(char character) {
    return character == ' ' || character == '\t' || character == '\n' ||
           character == '\r' || character == '\f' || character == '\v';
}

// Reports whether every byte in text is accepted ASCII whitespace.
bool only_ascii_whitespace(std::string_view text) {
    return std::all_of(text.begin(), text.end(), ascii_whitespace);
}

// Trims accepted ASCII whitespace from both ends without copying.
std::string_view trim_ascii_whitespace(std::string_view text) {
    while (!text.empty() && ascii_whitespace(text.front())) {
        text.remove_prefix(1U);
    }
    while (!text.empty() && ascii_whitespace(text.back())) {
        text.remove_suffix(1U);
    }
    return text;
}

// Reports whether every value byte is in the printable ASCII range.
bool printable_ascii(std::string_view value) {
    return std::all_of(value.begin(), value.end(), [](char character) {
        const auto byte = static_cast<std::uint8_t>(character);
        return byte >= first_printable_ascii && byte <= last_printable_ascii;
    });
}

}  // namespace

SerialNumberService::SerialNumberService(SerialNumberPort& port) : port_(port) {}

void SerialNumberService::handle_get(std::string_view command) {
    if (command.size() < get_command.size() ||
        command.substr(0U, get_command.size()) != get_command ||
        !only_ascii_whitespace(command.substr(get_command.size()))) {
        respond("The command format is invalid\n");
        return;
    }
    if (!port_.admit_operation(operation_wait_milliseconds)) {
        respond("SN_GET_ERR: busy\n");
        return;
    }

    const SerialNumberRead read =
        port_.read_serial(serial_namespace, serial_key);
    if (read.result == SerialNumberReadResult::missing_namespace ||
        read.result == SerialNumberReadResult::missing_key ||
        (read.result == SerialNumberReadResult::success &&
         read.value.empty())) {
        respond("sn = null\n");
    } else if (read.result != SerialNumberReadResult::success) {
        respond("sn get failed\n");
    } else {
        respond(std::string("sn = ") + read.value + "\n");
    }
    port_.complete_operation();
}

void SerialNumberService::handle_set(std::string_view command) {
    if (command.size() <= set_command.size() ||
        command.substr(0U, set_command.size()) != set_command ||
        !ascii_whitespace(command[set_command.size()])) {
        respond("The command format is invalid\n");
        return;
    }
    const std::string_view value =
        trim_ascii_whitespace(command.substr(set_command.size()));
    if (value.size() < minimum_serial_length ||
        value.size() > maximum_serial_length) {
        respond("Invalid len\n");
        return;
    }
    if (!printable_ascii(value)) {
        respond("There are some illegal characters in the sn\n");
        return;
    }
    if (!port_.admit_operation(operation_wait_milliseconds)) {
        respond("SN_SET_ERR: busy\n");
        return;
    }

    const SerialNumberRead existing =
        port_.read_serial(serial_namespace, serial_key);
    if (existing.result == SerialNumberReadResult::success &&
        !existing.value.empty()) {
        respond("The value of sn cannot be changed\n");
        port_.complete_operation();
        return;
    }
    if (!port_.write_serial(serial_namespace, serial_key, value)) {
        respond("sn set failed\n");
    } else {
        respond(std::string("Success:sn = ") + std::string(value) + "\n");
    }
    port_.complete_operation();
}

void SerialNumberService::respond(std::string_view payload) {
    port_.send_response(runtime_response_type, payload);
}

}  // namespace firmware::application
