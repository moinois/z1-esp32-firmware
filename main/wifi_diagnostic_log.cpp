// Implements the bounded Wi-Fi diagnostic log on top of NVS string storage.
#include "wifi_diagnostic_log.hpp"

#include "nvs_key_value_adapter.hpp"

#include <algorithm>

namespace firmware::target {
namespace {

constexpr std::string_view diagnostic_namespace = "wifi_diag";
constexpr std::string_view diagnostic_key = "events";
constexpr std::size_t maximum_log_size = 2048U;

std::string bounded_log(std::string value) {
    if (value.size() <= maximum_log_size) return value;
    const std::size_t start = value.size() - maximum_log_size;
    const std::size_t line = value.find('\n', start);
    return line == std::string::npos ? value.substr(start)
                                     : value.substr(line + 1U);
}

}  // namespace

bool WifiDiagnosticLog::append(std::string_view message) const {
    if (message.empty()) return true;
    const auto existing = NvsKeyValueAdapter{}.read_string(
        diagnostic_namespace, diagnostic_key);
    std::string value = existing.state == NvsReadState::found
                            ? existing.value
                            : std::string{};
    value.append(message);
    if (value.empty() || value.back() != '\n') value.push_back('\n');
    return NvsKeyValueAdapter{}.write_string(
        diagnostic_namespace, diagnostic_key, bounded_log(std::move(value)));
}

std::string WifiDiagnosticLog::read() const {
    const auto result = NvsKeyValueAdapter{}.read_string(
        diagnostic_namespace, diagnostic_key);
    return result.state == NvsReadState::found ? result.value : std::string{};
}

bool WifiDiagnosticLog::clear() const {
    const auto result = NvsKeyValueAdapter{}.erase_key(
        diagnostic_namespace, diagnostic_key);
    return result == NvsReadState::found || result == NvsReadState::missing;
}

WifiDiagnosticLog& wifi_diagnostic_log() {
    static WifiDiagnosticLog log;
    return log;
}

}  // namespace firmware::target
