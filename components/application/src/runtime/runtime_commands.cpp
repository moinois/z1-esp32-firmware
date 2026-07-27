// Implements persisted runtime reporting and first_boot-only clearing.
#include "firmware/application/runtime_commands.hpp"
#include "firmware/application/runtime_persistence.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::string_view system_time_command = "sys-time";
constexpr std::string_view clear_first_time_command = "clearftm";
constexpr std::uint32_t operation_wait_milliseconds = 200U;

// Reports whether one byte is accepted trailing ASCII whitespace.
bool ascii_whitespace(char character) {
    return character == ' ' || character == '\t' || character == '\n' ||
           character == '\r' || character == '\f' || character == '\v';
}

}  // namespace

RuntimeCommandService::RuntimeCommandService(RuntimeCommandPort& port)
    : port_(port) {}

void RuntimeCommandService::handle_system_time(std::string_view command) {
    if (!valid_shape(command, system_time_command)) {
        respond("The command format is invalid\n");
        return;
    }
    if (!port_.admit_operation(operation_wait_milliseconds)) {
        respond("sys-time: busy\n");
        return;
    }
    if (!port_.open_namespace(runtime_persistence::name_space)) {
        respond("sys-time-data get failed\n");
        respond("sys-time-data = null,0,0\n");
        port_.complete_operation();
        return;
    }

    const RuntimeSignedRead first_boot =
        port_.read_first_boot(runtime_persistence::first_boot_key);
    std::string timestamp = "null";
    if (first_boot.result == RuntimeValueResult::success) {
        timestamp = port_.format_utc_minute(first_boot.value).value_or("invalid");
    }
    const std::uint64_t power_on =
        port_.read_counter(runtime_persistence::power_on_seconds_key).value_or(0U);
    const std::uint64_t machine =
        port_.read_counter(runtime_persistence::machine_seconds_key).value_or(0U);
    respond(std::string("sys-time-data = ") + timestamp + "," +
            std::to_string(power_on) + "," + std::to_string(machine) + "\n");
    port_.complete_operation();
}

void RuntimeCommandService::handle_clear_first_boot(
    std::string_view command) {
    if (!valid_shape(command, clear_first_time_command)) {
        respond("The command format is invalid\n");
        return;
    }
    if (!port_.admit_operation(operation_wait_milliseconds)) {
        respond("clearftm: busy\n");
        return;
    }
    const RuntimeEraseResult result =
        port_.erase_first_boot(runtime_persistence::name_space,
                               runtime_persistence::first_boot_key);
    if (result == RuntimeEraseResult::success ||
        result == RuntimeEraseResult::missing) {
        respond("clearftm ok\n");
    } else {
        respond("clearftm failed\n");
    }
    port_.complete_operation();
}

bool RuntimeCommandService::valid_shape(
    std::string_view command, std::string_view exact_prefix) const {
    if (command.size() < exact_prefix.size() ||
        command.substr(0U, exact_prefix.size()) != exact_prefix) {
        return false;
    }
    const std::string_view suffix = command.substr(exact_prefix.size());
    return std::all_of(suffix.begin(), suffix.end(), ascii_whitespace);
}

void RuntimeCommandService::respond(std::string_view payload) {
    port_.send_response(core::protocol::text_response, payload);
}

}  // namespace firmware::application
