// Implements runtime command persistence and framed UART responses.
#include "controller_runtime_command_adapter.hpp"

#include "controller_uart_adapter.hpp"
#include "nvs_key_value_adapter.hpp"
#include "runtime_operation_capacity.hpp"
#include "firmware/core/frame.hpp"
#include "esp_log.h"

#include <ctime>

namespace firmware::target {

ControllerRuntimeCommandAdapter::ControllerRuntimeCommandAdapter(
    ControllerUartAdapter& uart)
    : uart_(uart) {}

bool ControllerRuntimeCommandAdapter::admit_operation(
    std::uint32_t wait_milliseconds) {
    return admit_runtime_operation(wait_milliseconds);
}

bool ControllerRuntimeCommandAdapter::open_namespace(std::string_view name_space) {
    name_space_ = std::string(name_space);
    return true;
}

firmware::application::RuntimeSignedRead
ControllerRuntimeCommandAdapter::read_first_boot(std::string_view key) {
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

std::optional<std::uint64_t> ControllerRuntimeCommandAdapter::read_counter(
    std::string_view key) {
    return NvsKeyValueAdapter{}.read_u64(name_space_, key);
}

std::optional<std::string> ControllerRuntimeCommandAdapter::format_utc_minute(
    std::int64_t seconds) {
    const time_t value = static_cast<time_t>(seconds);
    std::tm utc{};
    if (gmtime_r(&value, &utc) == nullptr) {
        return std::nullopt;
    }
    char buffer[32];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M UTC", &utc) == 0U) {
        return std::nullopt;
    }
    return std::string(buffer);
}

firmware::application::RuntimeEraseResult
ControllerRuntimeCommandAdapter::erase_first_boot(
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

void ControllerRuntimeCommandAdapter::complete_operation() {
    complete_runtime_operation();
}

void ControllerRuntimeCommandAdapter::send_response(std::uint8_t type,
                                                    std::string_view payload) {
    const firmware::core::Frame response{
        type, firmware::core::ByteVector(payload.begin(), payload.end())};
    const auto encoded = firmware::core::encode_frame(response);
    if (!encoded.empty()) {
        const int written = uart_.write(encoded);
        if (written != static_cast<int>(encoded.size())) {
            ESP_LOGE("uart_task", "UART send failed");
        }
    }
}

}  // namespace firmware::target
