// Implements NVS-backed serial-number commands for TCP-origin requests.
#include "tcp_serial_number_adapter.hpp"

#include "nvs_key_value_adapter.hpp"
#include "firmware/application/tcp_client_session.hpp"
#include "firmware/core/frame.hpp"

namespace firmware::target {

TcpSerialNumberAdapter::TcpSerialNumberAdapter(
    firmware::application::TcpClientSession& session)
    : session_(session) {}

bool TcpSerialNumberAdapter::admit_operation(std::uint32_t) {
    return true;
}

firmware::application::SerialNumberRead TcpSerialNumberAdapter::read_serial(
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

bool TcpSerialNumberAdapter::write_serial(std::string_view name_space,
                                          std::string_view key,
                                          std::string_view value) {
    NvsKeyValueAdapter nvs;
    return nvs.write_string(name_space, key, value);
}

void TcpSerialNumberAdapter::complete_operation() {}

void TcpSerialNumberAdapter::send_response(std::uint8_t type,
                                           std::string_view payload) {
    const firmware::core::Frame response{
        type, firmware::core::ByteVector(payload.begin(), payload.end())};
    static_cast<void>(session_.queue_frame(response));
}

}  // namespace firmware::target
