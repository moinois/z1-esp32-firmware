/** @file @brief Declares bounded diagnostic capture and SD-record formatting policy. */
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string_view>

namespace firmware::application {

/// Holds wall-clock fields and uptime sampled for one diagnostic record.
struct DiagnosticTime {
    std::uint16_t year;
    std::uint8_t month;
    std::uint8_t day;
    std::uint8_t hour;
    std::uint8_t minute;
    std::uint8_t second;
    std::uint16_t millisecond;
    std::uint64_t uptime_milliseconds;
};

/// Formats UTC calendar time when valid and otherwise the uptime fallback.
std::string format_diagnostic_timestamp(const DiagnosticTime& time);

/// Identifies capture contexts that must bypass SD buffering.
struct CaptureContext {
    bool recursive = false;
    bool critically_low_stack = false;
};

/// Isolates capture policy from the prior output, buffer pool, and clock.
class DiagnosticCapturePort {
public:
    /// Enables safe destruction through a substituted port implementation.
    virtual ~DiagnosticCapturePort() = default;

    /// Writes bounded text to the prior diagnostic output destination.
    virtual void print(core::BytesView message) = 0;

    /// Reports whether one record buffer can be acquired without blocking.
    virtual bool record_buffer_available() = 0;

    /// Returns the timestamp and uptime snapshot for one stored record.
    virtual DiagnosticTime current_time() = 0;
};

/// Prints every capture and optionally queues a bounded SD-log copy.
class DiagnosticCapture {
public:
    /// Enables or disables creation of new SD records without discarding pending ones.
    void set_logging_active(bool active);

    /// Prints and conditionally stores one already-formatted diagnostic message.
    std::size_t capture(std::string_view message, CaptureContext context,
                        DiagnosticCapturePort& port);

    /// Removes and returns the oldest pending formatted SD record.
    std::optional<core::ByteVector> take_pending();

    /// Reports the number of records awaiting the log writer.
    std::size_t pending_count() const;

private:
    std::deque<core::ByteVector> pending_;
    bool logging_active_ = false;
};

}  // namespace firmware::application
