// Declares opt-in SD mirroring of UART diagnostics to a bounded serial.log.
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {

inline constexpr std::uint64_t serial_diagnostic_log_maximum_size =
    384U * 1024U;

// Isolates serial.log policy from target filesystem streams.
class SerialDiagnosticLogPort {
public:
    // Enables safe destruction through a substituted port implementation.
    virtual ~SerialDiagnosticLogPort() = default;

    // Opens an existing sentinel file for append and returns its current size.
    virtual std::optional<std::uint64_t> open_existing_append(
        std::string_view path) = 0;

    // Writes one complete diagnostic record and returns accepted bytes.
    virtual std::size_t write(core::BytesView record) = 0;

    // Flushes accepted bytes for maximum crash-time visibility.
    virtual void flush() = 0;

    // Closes the optional mirror stream.
    virtual void close() = 0;
};

// Appends diagnostic records only while the explicit sentinel has capacity.
class SerialDiagnosticLogWriter {
public:
    // Opens the sentinel on demand and appends one size-bounded record.
    void write_record(core::BytesView record, SerialDiagnosticLogPort& port);

    // Closes the stream when storage is unmounted or logging stops.
    void disable(SerialDiagnosticLogPort& port);

    // Reports whether the optional mirror stream is currently open.
    bool active() const;

    // Reports bytes accepted in the current or most recent sentinel file.
    std::uint64_t tracked_size() const;

    // Reports records rejected by capacity or a short filesystem write.
    std::uint64_t dropped_record_count() const;

private:
    bool active_ = false;
    std::uint64_t tracked_size_ = 0U;
    std::uint64_t dropped_record_count_ = 0U;
};

}  // namespace firmware::application
