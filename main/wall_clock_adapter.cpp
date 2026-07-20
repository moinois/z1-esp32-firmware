// Implements wall-clock access through POSIX time APIs and runtime notification.
#include "wall_clock_adapter.hpp"

#include "esp_log.h"

#include "runtime_counter_task.hpp"
#include "controller_uart_adapter.hpp"

#include "firmware/core/frame.hpp"

#include <ctime>
#include <sys/time.h>

namespace firmware::target {

EspWallClockAdapter::EspWallClockAdapter(ControllerUartAdapter* uart)
    : uart_(uart) {}

std::int64_t EspWallClockAdapter::unix_seconds() const {
    return static_cast<std::int64_t>(std::time(nullptr));
}

bool EspWallClockAdapter::set_time(std::int64_t seconds,
                                   std::int32_t microseconds) {
    const timeval value{static_cast<time_t>(seconds), microseconds};
    return settimeofday(&value, nullptr) == 0;
}

std::optional<std::string> EspWallClockAdapter::format_utc(
    std::int64_t seconds) {
    const time_t value = static_cast<time_t>(seconds);
    std::tm utc{};
    if (gmtime_r(&value, &utc) == nullptr) return std::nullopt;
    char buffer[32];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", &utc) == 0U) {
        return std::nullopt;
    }
    return std::string(buffer);
}

void EspWallClockAdapter::request_first_boot_recording() {
    record_runtime_first_boot(unix_seconds());
}

void EspWallClockAdapter::log_info(std::string_view tag,
                                   std::string_view message) {
    ESP_LOGI(std::string(tag).c_str(), "%s", std::string(message).c_str());
}

void EspWallClockAdapter::log_error(std::string_view tag,
                                    std::string_view message) {
    ESP_LOGE(std::string(tag).c_str(), "%s", std::string(message).c_str());
}

void EspWallClockAdapter::send_response(std::uint8_t type,
                                        std::string_view payload) {
    if (uart_ == nullptr) return;
    const firmware::core::Frame response{
        type,
        firmware::core::ByteVector(payload.begin(), payload.end())};
    const auto encoded = firmware::core::encode_frame(response);
    if (!encoded.empty()) uart_->write(encoded);
}

}  // namespace firmware::target
