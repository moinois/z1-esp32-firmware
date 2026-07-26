// Verifies wall-clock command parsing, protocol silence, and diagnostics.
#include "test.hpp"

#include "firmware/application/wall_clock.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::WallClockPort;
using firmware::application::WallClockService;

namespace {

// Records clock access, first-boot requests, diagnostics, and host responses.
class FakeWallClockPort final : public WallClockPort {
public:
    // Returns the configured current Unix seconds.
    std::int64_t unix_seconds() const override {
        return current_seconds;
    }

    // Records the requested seconds and microseconds, then returns its outcome.
    bool set_time(std::int64_t seconds, std::int32_t microseconds) override {
        set_seconds.push_back(seconds);
        set_microseconds.push_back(microseconds);
        return set_succeeds;
    }

    // Returns the configured UTC rendering outcome.
    std::optional<std::string> format_utc(std::int64_t seconds) override {
        formatted_seconds.push_back(seconds);
        return utc_text;
    }

    // Records one best-effort first-boot persistence request.
    void request_first_boot_recording() override {
        calls.emplace_back("first-boot");
    }

    // Records one informational application diagnostic.
    void log_info(std::string_view tag, std::string_view message) override {
        calls.emplace_back("info");
        log_tags.emplace_back(tag);
        logs.emplace_back(message);
    }

    // Records one error application diagnostic.
    void log_error(std::string_view tag, std::string_view message) override {
        calls.emplace_back("error");
        log_tags.emplace_back(tag);
        logs.emplace_back(message);
    }

    // Records one routed runtime response packet.
    void send_response(std::uint8_t type, std::string_view payload) override {
        response_types.push_back(type);
        responses.emplace_back(payload);
    }

    std::int64_t current_seconds = 1234;
    bool set_succeeds = true;
    std::optional<std::string> utc_text = "2025-01-02 03:04:05";
    std::vector<std::string> calls;
    std::vector<std::int64_t> set_seconds;
    std::vector<std::int32_t> set_microseconds;
    std::vector<std::int64_t> formatted_seconds;
    std::vector<std::string> log_tags;
    std::vector<std::string> logs;
    std::vector<std::uint8_t> response_types;
    std::vector<std::string> responses;
};

}  // namespace

TEST_CASE(run_020_exact_time_command_returns_current_seconds) {
    FakeWallClockPort port;
    WallClockService clock(port);

    clock.handle("time");

    REQUIRE_EQ(port.response_types, std::vector<std::uint8_t>({0x83U}));
    REQUIRE_EQ(port.responses,
               std::vector<std::string>({"time = 1234\r\n"}));
    REQUIRE(port.set_seconds.empty());
}

TEST_CASE(run_021_invalid_time_arguments_are_ignored_without_reply) {
    FakeWallClockPort port;
    WallClockService clock(port);

    for (const std::string_view command :
         {"time ", "time 0", "time -1", "time 12x",
          "time 9223372036854775808", "timer 12"}) {
        clock.handle(command);
    }

    REQUIRE(port.responses.empty());
    REQUIRE(port.set_seconds.empty());
    REQUIRE(port.logs.empty());
}

TEST_CASE(run_021_positive_decimal_accepts_an_explicit_plus_sign) {
    FakeWallClockPort port;
    WallClockService clock(port);

    clock.handle("time +1");

    REQUIRE_EQ(port.set_seconds, std::vector<std::int64_t>({1}));
}

TEST_CASE(run_021_to_023_valid_time_sets_zero_microseconds_then_records_boot) {
    FakeWallClockPort port;
    WallClockService clock(port);

    clock.handle("time  9223372036854775807 \r\n");

    REQUIRE_EQ(port.set_seconds,
               std::vector<std::int64_t>({9223372036854775807LL}));
    REQUIRE_EQ(port.set_microseconds, std::vector<std::int32_t>({0}));
    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"first-boot", "info"}));
    REQUIRE_EQ(port.log_tags, std::vector<std::string>({"APP_FILE"}));
    REQUIRE_EQ(
        port.logs,
        std::vector<std::string>({
            "Time set successfully: 9223372036854775807 (2025-01-02 03:04:05)"}));
    REQUIRE(port.responses.empty());
}

TEST_CASE(diag_020_success_uses_seconds_when_utc_conversion_fails) {
    FakeWallClockPort port;
    port.utc_text = std::nullopt;
    WallClockService clock(port);

    clock.handle("time 42");

    REQUIRE_EQ(port.logs,
               std::vector<std::string>({
                   "Time set successfully: 42 (42)"}));
    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"first-boot", "info"}));
}

TEST_CASE(run_022_and_diag_020_set_failure_is_silent_but_logs_error) {
    FakeWallClockPort port;
    port.set_succeeds = false;
    WallClockService clock(port);

    clock.handle("time 42");

    REQUIRE_EQ(port.calls, std::vector<std::string>({"error"}));
    REQUIRE_EQ(port.log_tags, std::vector<std::string>({"APP_FILE"}));
    REQUIRE_EQ(port.logs,
               std::vector<std::string>({"Failed to set time: 42"}));
    REQUIRE(port.responses.empty());
    REQUIRE(port.formatted_seconds.empty());
}
