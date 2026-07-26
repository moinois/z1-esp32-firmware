// Implements common NVS persistence while delegating response delivery.
#include "nvs_command_ports.hpp"

#include "nvs_key_value_adapter.hpp"
#include "runtime_operation_capacity.hpp"

#include <ctime>

namespace firmware::target {

NvsRuntimeCommandPort::NvsRuntimeCommandPort(FrameSink& sink) : sink_(sink) {}

bool NvsRuntimeCommandPort::admit_operation(std::uint32_t wait_milliseconds) {
    return admit_runtime_operation(wait_milliseconds);
}

bool NvsRuntimeCommandPort::open_namespace(std::string_view name_space) {
    name_space_ = std::string(name_space);
    return true;
}

firmware::application::RuntimeSignedRead NvsRuntimeCommandPort::read_first_boot(
    std::string_view key) {
    const auto value = NvsKeyValueAdapter{}.read_u64_state(name_space_, key);
    if (value.state == NvsReadState::found) {
        return {firmware::application::RuntimeValueResult::success,
                static_cast<std::int64_t>(value.value)};
    }
    if (value.state == NvsReadState::missing) {
        return {firmware::application::RuntimeValueResult::missing, 0};
    }
    return {firmware::application::RuntimeValueResult::failure, 0};
}

std::optional<std::uint64_t> NvsRuntimeCommandPort::read_counter(
    std::string_view key) {
    return NvsKeyValueAdapter{}.read_u64(name_space_, key);
}

std::optional<std::string> NvsRuntimeCommandPort::format_utc_minute(
    std::int64_t seconds) {
    const time_t value = static_cast<time_t>(seconds);
    std::tm utc{};
    if (gmtime_r(&value, &utc) == nullptr) return std::nullopt;
    char buffer[32];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M UTC", &utc) == 0U) {
        return std::nullopt;
    }
    return std::string(buffer);
}

firmware::application::RuntimeEraseResult NvsRuntimeCommandPort::erase_first_boot(
    std::string_view name_space, std::string_view key) {
    const auto result = NvsKeyValueAdapter{}.erase_key(name_space, key);
    if (result == NvsReadState::found) {
        return firmware::application::RuntimeEraseResult::success;
    }
    if (result == NvsReadState::missing) {
        return firmware::application::RuntimeEraseResult::missing;
    }
    return firmware::application::RuntimeEraseResult::failure;
}

void NvsRuntimeCommandPort::complete_operation() {
    complete_runtime_operation();
}

void NvsRuntimeCommandPort::send_response(std::uint8_t type,
                                          std::string_view payload) {
    static_cast<void>(sink_.send_frame(
        {type, firmware::core::ByteVector(payload.begin(), payload.end())}));
}

NvsSerialNumberPort::NvsSerialNumberPort(FrameSink& sink) : sink_(sink) {}

bool NvsSerialNumberPort::admit_operation(std::uint32_t wait_milliseconds) {
    return admit_runtime_operation(wait_milliseconds);
}

firmware::application::SerialNumberRead NvsSerialNumberPort::read_serial(
    std::string_view name_space, std::string_view key) {
    const auto result = NvsKeyValueAdapter{}.read_string(name_space, key);
    if (result.state == NvsReadState::found) {
        return {firmware::application::SerialNumberReadResult::success,
                result.value};
    }
    if (result.state == NvsReadState::missing) {
        return {firmware::application::SerialNumberReadResult::missing_key, {}};
    }
    return {firmware::application::SerialNumberReadResult::failure, {}};
}

bool NvsSerialNumberPort::write_serial(std::string_view name_space,
                                       std::string_view key,
                                       std::string_view value) {
    return NvsKeyValueAdapter{}.write_string(name_space, key, value);
}

void NvsSerialNumberPort::complete_operation() {
    complete_runtime_operation();
}

void NvsSerialNumberPort::send_response(std::uint8_t type,
                                        std::string_view payload) {
    static_cast<void>(sink_.send_frame(
        {type, firmware::core::ByteVector(payload.begin(), payload.end())}));
}

}  // namespace firmware::target
