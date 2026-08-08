/** @file @brief Declares SD diagnostic log writing, rotation, flushing, and shutdown policy. */
#pragma once

#include "firmware/application/diagnostic_capture.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {

/// Isolates log-writer policy from buffered filesystem and clock APIs.
class DiagnosticLogPort {
public:
    /// Enables safe destruction through a substituted port implementation.
    virtual ~DiagnosticLogPort() = default;

    /// Opens a file for append with the requested stream buffer and returns size.
    virtual std::optional<std::uint64_t> open_append(
        std::string_view path, std::size_t buffer_size) = 0;

    /// Writes once and returns only the bytes actually accepted.
    virtual std::size_t write(core::BytesView record) = 0;

    /// Flushes the current buffered log stream.
    virtual void flush() = 0;

    /// Closes the current buffered log stream.
    virtual void close() = 0;

    /// Attempts one best-effort file removal during rotation.
    virtual void remove_file(std::string_view path) = 0;

    /// Attempts one best-effort file rename during rotation.
    virtual void rename_file(std::string_view source,
                             std::string_view destination) = 0;

    /// Returns the current UTC/uptime snapshot for a session marker.
    virtual DiagnosticTime current_time() = 0;
};

/// Owns one SD log session and consumes formatted capture records.
class DiagnosticLogWriter {
public:
    /// Opens a session, appends its marker, and flushes it immediately.
    bool open_session(std::uint64_t now_milliseconds,
                      DiagnosticLogPort& port);

    /// Appends and flushes the system-time synchronization marker.
    void system_time_synced(DiagnosticLogPort& port);

    /// Writes or discards one record, rotating first when required.
    void write_record(core::BytesView record, DiagnosticLogPort& port);

    /// Applies the strict periodic flush threshold.
    void poll(std::uint64_t now_milliseconds, DiagnosticLogPort& port);

    /// Starts the bounded shutdown drain window.
    void begin_shutdown(std::uint64_t now_milliseconds);

    /// Drains one record or forces flush and close when the window expires.
    void poll_shutdown(std::uint64_t now_milliseconds,
                       DiagnosticCapture& capture,
                       DiagnosticLogPort& port);

    /// Reports whether a log file remains open for writing.
    bool active() const;

    /// Reports bytes actually written to the current tracked file.
    std::uint64_t tracked_size() const;

private:
    bool rotate(DiagnosticLogPort& port);
    void write_marker(std::string_view label, DiagnosticLogPort& port);
    void flush_and_close(DiagnosticLogPort& port);

    std::uint64_t tracked_size_ = 0U;
    std::uint64_t last_flush_milliseconds_ = 0U;
    std::uint64_t shutdown_started_milliseconds_ = 0U;
    bool active_ = false;
    bool shutting_down_ = false;
};

}  // namespace firmware::application
