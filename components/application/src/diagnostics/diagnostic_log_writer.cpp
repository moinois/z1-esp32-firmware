/** @file @brief Implements log session markers, tracked writes, rotation, and bounded shutdown. */
#include "application/diagnostics/diagnostic_log_writer.hpp"
#include "core/filesystem/sd_user_path.hpp"

#include <string>

namespace firmware::application {
namespace {

const std::string log_path = core::physical_sd_path("/.logs/GeneralInfo.log");
const std::string first_rotated_path =
    core::physical_sd_path("/.logs/GeneralInfo.log.1");
const std::string second_rotated_path =
    core::physical_sd_path("/.logs/GeneralInfo.log.2");
constexpr std::size_t file_buffer_size = 4096U;
constexpr std::uint64_t rotation_size_limit = 0x300000U;
constexpr std::uint64_t periodic_flush_threshold_milliseconds = 1990U;
constexpr std::uint64_t shutdown_timeout_milliseconds = 5000U;

}  // namespace

bool DiagnosticLogWriter::open_session(std::uint64_t now_milliseconds,
                                       DiagnosticLogPort& port) {
    const auto size = port.open_append(log_path, file_buffer_size);
    if (!size.has_value()) {
        active_ = false;
        return false;
    }
    tracked_size_ = *size;
    last_flush_milliseconds_ = now_milliseconds;
    shutting_down_ = false;
    active_ = true;
    write_marker("log session start", port);
    return true;
}

void DiagnosticLogWriter::system_time_synced(DiagnosticLogPort& port) {
    if (active_) {
        write_marker("system time synced", port);
    }
}

void DiagnosticLogWriter::write_record(core::BytesView record,
                                       DiagnosticLogPort& port) {
    if (!active_) {
        return;
    }
    if (tracked_size_ + record.size() > rotation_size_limit && !rotate(port)) {
        return;
    }
    tracked_size_ += port.write(record);
}

void DiagnosticLogWriter::poll(std::uint64_t now_milliseconds,
                               DiagnosticLogPort& port) {
    if (!active_ ||
        now_milliseconds - last_flush_milliseconds_ <=
            periodic_flush_threshold_milliseconds) {
        return;
    }
    port.flush();
    last_flush_milliseconds_ = now_milliseconds;
}

void DiagnosticLogWriter::begin_shutdown(std::uint64_t now_milliseconds) {
    shutting_down_ = true;
    shutdown_started_milliseconds_ = now_milliseconds;
}

void DiagnosticLogWriter::poll_shutdown(std::uint64_t now_milliseconds,
                                        DiagnosticCapture& capture,
                                        DiagnosticLogPort& port) {
    if (!shutting_down_) {
        return;
    }
    if (now_milliseconds - shutdown_started_milliseconds_ >=
        shutdown_timeout_milliseconds) {
        flush_and_close(port);
        while (capture.take_pending().has_value()) {
            // Timed-out records must not leak into a later card session.
        }
        return;
    }
    const auto record = capture.take_pending();
    if (record.has_value()) {
        write_record(*record, port);
        return;
    }
    flush_and_close(port);
}

bool DiagnosticLogWriter::active() const {
    return active_;
}

std::uint64_t DiagnosticLogWriter::tracked_size() const {
    return tracked_size_;
}

bool DiagnosticLogWriter::rotate(DiagnosticLogPort& port) {
    port.close();
    port.remove_file(second_rotated_path);
    port.rename_file(first_rotated_path, second_rotated_path);
    port.rename_file(log_path, first_rotated_path);
    const auto size = port.open_append(log_path, file_buffer_size);
    if (!size.has_value()) {
        active_ = false;
        return false;
    }
    tracked_size_ = *size;
    return true;
}

void DiagnosticLogWriter::write_marker(std::string_view label,
                                       DiagnosticLogPort& port) {
    const std::string marker = "===== " + std::string(label) + " [" +
                               format_diagnostic_timestamp(port.current_time()) +
                               "] =====\r\n";
    tracked_size_ += port.write(
        {reinterpret_cast<const std::uint8_t*>(marker.data()), marker.size()});
    port.flush();
}

void DiagnosticLogWriter::flush_and_close(DiagnosticLogPort& port) {
    if (active_) {
        port.flush();
        port.close();
    }
    active_ = false;
    shutting_down_ = false;
}

}  // namespace firmware::application
