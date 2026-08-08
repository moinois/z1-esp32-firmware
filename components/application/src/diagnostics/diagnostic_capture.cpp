/** @file @brief Implements bounded output, ANSI removal, timestamps, and nonblocking queueing. */
#include "firmware/application/diagnostic_capture.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace firmware::application {
namespace {

constexpr std::size_t formatted_buffer_size = 512U;
constexpr std::size_t maximum_printed_size = formatted_buffer_size - 1U;
constexpr std::size_t maximum_pending_records = 48U;
constexpr std::uint16_t first_valid_utc_year = 2020U;
constexpr std::uint8_t escape_byte = 0x1BU;

// Removes terminal CSI and simple two-byte ANSI escape sequences.
std::string remove_ansi(std::string_view message) {
    std::string plain;
    plain.reserve(message.size());
    std::size_t cursor = 0U;
    while (cursor < message.size()) {
        const auto byte = static_cast<std::uint8_t>(message[cursor]);
        if (byte != escape_byte) {
            plain.push_back(message[cursor]);
            ++cursor;
            continue;
        }
        ++cursor;
        if (cursor == message.size()) {
            break;
        }
        if (message[cursor] != '[') {
            ++cursor;
            continue;
        }
        ++cursor;
        while (cursor < message.size()) {
            const auto control = static_cast<std::uint8_t>(message[cursor++]);
            if (control >= 0x40U && control <= 0x7EU) {
                break;
            }
        }
    }
    return plain;
}

}  // namespace

std::string format_diagnostic_timestamp(const DiagnosticTime& time) {
    char timestamp[40];
    int length = 0;
    if (time.year >= first_valid_utc_year) {
        length = std::snprintf(timestamp, sizeof(timestamp),
                               "%04u-%02u-%02u %02u:%02u:%02u.%03u",
                               static_cast<unsigned>(time.year),
                               static_cast<unsigned>(time.month),
                               static_cast<unsigned>(time.day),
                               static_cast<unsigned>(time.hour),
                               static_cast<unsigned>(time.minute),
                               static_cast<unsigned>(time.second),
                               static_cast<unsigned>(time.millisecond));
    } else {
        length = std::snprintf(timestamp, sizeof(timestamp),
                               "uptime %llu ms",
                               static_cast<unsigned long long>(
                                   time.uptime_milliseconds));
    }
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(timestamp)) {
        return {};
    }
    return {timestamp, static_cast<std::size_t>(length)};
}

void DiagnosticCapture::set_logging_active(bool active) {
    logging_active_ = active;
}

std::size_t DiagnosticCapture::capture(std::string_view message,
                                       CaptureContext context,
                                       DiagnosticCapturePort& port) {
    const std::size_t untruncated_length = message.size();
    const std::size_t printed_size =
        std::min(message.size(), maximum_printed_size);
    core::ByteVector printed(message.begin(), message.begin() +
                                                 static_cast<std::ptrdiff_t>(printed_size));
    port.print(printed);

    const bool bypass = context.recursive || context.critically_low_stack;
    if (!logging_active_ || bypass || pending_.size() >= maximum_pending_records ||
        !port.record_buffer_available()) {
        return untruncated_length;
    }

    const std::string bounded_text(printed.begin(), printed.end());
    const std::string timestamp =
        format_diagnostic_timestamp(port.current_time());
    std::string record = "[" + timestamp + "] " + remove_ansi(bounded_text);
    pending_.emplace_back(record.begin(), record.end());
    return untruncated_length;
}

std::optional<core::ByteVector> DiagnosticCapture::take_pending() {
    if (pending_.empty()) {
        return std::nullopt;
    }
    core::ByteVector record = std::move(pending_.front());
    pending_.pop_front();
    return record;
}

std::size_t DiagnosticCapture::pending_count() const {
    return pending_.size();
}

}  // namespace firmware::application
