/** @file @brief Declares the ESP-IDF system-clock implementation of WallClockPort. */
#pragma once

#include "firmware/application/wall_clock.hpp"

namespace firmware::target {

class ControllerChannelAdapter;

/** Implements UTC reads/sets and controller responses using POSIX target time. */
class EspWallClockAdapter final : public firmware::application::WallClockPort {
public:
    /// Binds wall-clock responses to the controller UART transport.
    explicit EspWallClockAdapter(ControllerChannelAdapter* channel = nullptr);
    std::int64_t unix_seconds() const override;
    bool set_time(std::int64_t seconds, std::int32_t microseconds) override;
    std::optional<std::string> format_utc(std::int64_t seconds) override;
    void request_first_boot_recording() override;
    void log_info(std::string_view tag, std::string_view message) override;
    void log_error(std::string_view tag, std::string_view message) override;
    void send_response(std::uint8_t type, std::string_view payload) override;

private:
    ControllerChannelAdapter* channel_ = nullptr;
};

}  // namespace firmware::target
