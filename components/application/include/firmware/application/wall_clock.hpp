/** @file @brief Declares wall-clock get/set command parsing, side effects, and diagnostics. */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::application {

/// Isolates wall-clock policy from system time, runtime work, logs, and routing.
class WallClockPort {
public:
    /// Enables safe destruction through a substituted wall-clock adapter.
    virtual ~WallClockPort() = default;

    /// Returns current wall-clock Unix seconds.
    virtual std::int64_t unix_seconds() const = 0;

    /// Sets wall-clock seconds and microseconds to the requested values.
    virtual bool set_time(std::int64_t seconds,
                          std::int32_t microseconds) = 0;

    /// Formats one accepted Unix value as UTC or reports conversion failure.
    virtual std::optional<std::string> format_utc(std::int64_t seconds) = 0;

    /// Best-effort requests first-boot persistence through runtime capacity.
    virtual void request_first_boot_recording() = 0;

    /// Emits one informational application diagnostic.
    virtual void log_info(std::string_view tag, std::string_view message) = 0;

    /// Emits one error application diagnostic.
    virtual void log_error(std::string_view tag, std::string_view message) = 0;

    /// Sends one response through origin-aware runtime routing.
    virtual void send_response(std::uint8_t type,
                               std::string_view payload) = 0;
};

/// Implements exact time query and silent positive-decimal time setting.
class WallClockService {
public:
    /// Binds clock command policy to replaceable time and diagnostic adapters.
    explicit WallClockService(WallClockPort& port);

    /// Handles one complete time-prefix command.
    void handle(std::string_view command);

private:
    /// Applies one already parsed positive Unix-seconds value.
    void set_time(std::int64_t seconds);

    WallClockPort& port_;
};

}  // namespace firmware::application
