// Implements serial-number persistence and framed UART response transport.
#include "serial_number_adapter.hpp"

#include "controller_uart_adapter.hpp"
#include "nvs_key_value_adapter.hpp"
#include "runtime_operation_capacity.hpp"

#include "firmware/core/frame.hpp"

namespace firmware::target {

NvsSerialNumberAdapter::NvsSerialNumberAdapter(ControllerUartAdapter* uart)
    : uart_(uart) {}

bool NvsSerialNumberAdapter::admit_operation(std::uint32_t wait_milliseconds) {
    return admit_runtime_operation(wait_milliseconds);
}

firmware::application::SerialNumberRead NvsSerialNumberAdapter::read_serial(
    std::string_view name_space, std::string_view key) {
    NvsKeyValueAdapter nvs;
    const auto result = nvs.read_string(name_space, key);
    if (result.state == NvsReadState::found) {
        return {firmware::application::SerialNumberReadResult::success,
                result.value};
    }
    if (result.state == NvsReadState::missing) {
        return {firmware::application::SerialNumberReadResult::missing_key, {}};
    }
    return {firmware::application::SerialNumberReadResult::failure, {}};
}

bool NvsSerialNumberAdapter::write_serial(std::string_view name_space,
                                          std::string_view key,
                                          std::string_view value) {
    NvsKeyValueAdapter nvs;
    return nvs.write_string(name_space, key, value);
}

void NvsSerialNumberAdapter::complete_operation() {
    complete_runtime_operation();
}

void NvsSerialNumberAdapter::send_response(std::uint8_t type,
                                           std::string_view payload) {
    if (uart_ == nullptr) return;
    const firmware::core::Frame response{
        type, firmware::core::ByteVector(payload.begin(), payload.end())};
    const auto encoded = firmware::core::encode_frame(response);
    if (!encoded.empty()) uart_->write(encoded);
}

}  // namespace firmware::target
