/** @file @brief Implements exact time queries, positive decimal parsing, and clock logs. */
#include "firmware/application/wall_clock.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

namespace firmware::application {
namespace {

constexpr std::string_view time_prefix = "time";
constexpr std::string_view diagnostic_tag = "APP_FILE";
constexpr std::int32_t zero_microseconds = 0;
constexpr std::uint8_t runtime_response_type = 0x83U;

// Reports whether one byte is accepted ASCII whitespace.
bool ascii_whitespace(char character) {
    return character == ' ' || character == '\t' || character == '\n' ||
           character == '\r' || character == '\f' || character == '\v';
}

// Trims accepted ASCII whitespace from both ends without allocating.
std::string_view trim_ascii_whitespace(std::string_view text) {
    while (!text.empty() && ascii_whitespace(text.front())) {
        text.remove_prefix(1U);
    }
    while (!text.empty() && ascii_whitespace(text.back())) {
        text.remove_suffix(1U);
    }
    return text;
}

// Parses a complete positive signed-64-bit decimal, including optional plus.
std::optional<std::int64_t> parse_positive_seconds(std::string_view text) {
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1U);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::int64_t seconds = 0;
    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), seconds, 10);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        seconds <= 0) {
        return std::nullopt;
    }
    return seconds;
}

}  // namespace

WallClockService::WallClockService(WallClockPort& port) : port_(port) {}

void WallClockService::handle(std::string_view command) {
    if (command == time_prefix) {
        port_.send_response(runtime_response_type,
                            std::string("time = ") +
                                std::to_string(port_.unix_seconds()) + "\r\n");
        return;
    }
    if (command.size() < time_prefix.size() ||
        command.substr(0U, time_prefix.size()) != time_prefix) {
        return;
    }
    const auto seconds = parse_positive_seconds(
        trim_ascii_whitespace(command.substr(time_prefix.size())));
    if (seconds.has_value()) {
        set_time(*seconds);
    }
}

void WallClockService::set_time(std::int64_t seconds) {
    if (!port_.set_time(seconds, zero_microseconds)) {
        port_.log_error(diagnostic_tag,
                        std::string("Failed to set time: ") +
                            std::to_string(seconds));
        return;
    }

    port_.request_first_boot_recording();
    const auto utc = port_.format_utc(seconds);
    const std::string rendered =
        utc.has_value() ? *utc : std::to_string(seconds);
    port_.log_info(diagnostic_tag,
                   std::string("Time set successfully: ") +
                       std::to_string(seconds) + " (" + rendered + ")");
}

}  // namespace firmware::application
