// Implements persisted runtime command behavior for TCP-origin requests.
#include "tcp_runtime_command_adapter.hpp"

#include "nvs_key_value_adapter.hpp"
#include "firmware/application/tcp_client_session.hpp"
#include "firmware/core/frame.hpp"

#include <ctime>

namespace firmware::target {

TcpRuntimeCommandAdapter::TcpRuntimeCommandAdapter(
    firmware::application::TcpClientSession& session)
    : session_(session) {}

bool TcpRuntimeCommandAdapter::admit_operation(std::uint32_t) {
    return true;
}

bool TcpRuntimeCommandAdapter::open_namespace(std::string_view name_space) {
    name_space_ = std::string(name_space);
    return true;
}

firmware::application::RuntimeSignedRead
TcpRuntimeCommandAdapter::read_first_boot(std::string_view key) {
    NvsKeyValueAdapter nvs;
    const auto value = nvs.read_u64_state(name_space_, key);
    if (value.state == NvsReadState::found) {
        return {firmware::application::RuntimeValueResult::success,
                static_cast<std::int64_t>(value.value)};
    }
    if (value.state == NvsReadState::missing) {
        return {firmware::application::RuntimeValueResult::missing, 0};
    }
    return {firmware::application::RuntimeValueResult::failure, 0};
}

std::optional<std::uint64_t> TcpRuntimeCommandAdapter::read_counter(
    std::string_view key) {
    NvsKeyValueAdapter nvs;
    return nvs.read_u64(name_space_, key);
}

std::optional<std::string> TcpRuntimeCommandAdapter::format_utc_minute(
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
TcpRuntimeCommandAdapter::erase_first_boot(std::string_view name_space,
                                           std::string_view key) {
    NvsKeyValueAdapter nvs;
    const auto result = nvs.erase_key(name_space, key);
    if (result == NvsReadState::found) {
        return firmware::application::RuntimeEraseResult::success;
    }
    if (result == NvsReadState::missing) {
        return firmware::application::RuntimeEraseResult::missing;
    }
    return firmware::application::RuntimeEraseResult::failure;
}

void TcpRuntimeCommandAdapter::complete_operation() {}

void TcpRuntimeCommandAdapter::send_response(std::uint8_t type,
                                             std::string_view payload) {
    const firmware::core::Frame response{
        type, firmware::core::ByteVector(payload.begin(), payload.end())};
    static_cast<void>(session_.queue_frame(response));
}

}  // namespace firmware::target
