// Verifies diagnostic log sessions, flushing, rotation, writes, and shutdown.
#include "test.hpp"

#include "application/diagnostics/diagnostic_log_writer.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::DiagnosticCapture;
using firmware::application::DiagnosticCapturePort;
using firmware::application::DiagnosticLogPort;
using firmware::application::DiagnosticLogWriter;
using firmware::application::DiagnosticTime;
using firmware::core::ByteVector;

namespace {

// Converts retained bytes to text for exact assertions.
std::string text(const ByteVector& value) {
    return {value.begin(), value.end()};
}

// Records file lifecycle and returns configurable write and open results.
class FakeDiagnosticLogPort final : public DiagnosticLogPort {
public:
    // Opens the current log and returns its configured existing size.
    std::optional<std::uint64_t> open_append(
        std::string_view path, std::size_t buffer_size) override {
        events.emplace_back("open");
        opened_path = path;
        requested_buffer_size = buffer_size;
        return open_result;
    }

    // Records a write and returns its configured or complete byte count.
    std::size_t write(firmware::core::BytesView record) override {
        events.emplace_back("write");
        writes.emplace_back(record.begin(), record.end());
        return next_write_size.has_value() ? *next_write_size : record.size();
    }

    // Records one explicit flush.
    void flush() override {
        events.emplace_back("flush");
    }

    // Records closure of the current log file.
    void close() override {
        events.emplace_back("close");
    }

    // Records best-effort removal during rotation.
    void remove_file(std::string_view path) override {
        events.emplace_back("remove:" + std::string(path));
    }

    // Records best-effort rename during rotation.
    void rename_file(std::string_view source,
                     std::string_view destination) override {
        events.emplace_back("rename:" + std::string(source) + ":" +
                            std::string(destination));
    }

    // Returns the configured timestamp snapshot.
    DiagnosticTime current_time() override {
        return time;
    }

    std::optional<std::uint64_t> open_result = 0U;
    std::optional<std::size_t> next_write_size;
    DiagnosticTime time{2026U, 7U, 20U, 1U, 2U, 3U, 4U, 100U};
    std::string opened_path;
    std::size_t requested_buffer_size = 0U;
    std::vector<std::string> events;
    std::vector<ByteVector> writes;
};

// Supplies pending capture records for shutdown-drain tests.
class FakeCapturePort final : public DiagnosticCapturePort {
public:
    // Ignores prior-output bytes for writer tests.
    void print(firmware::core::BytesView) override {}

    // Always permits a pending record.
    bool record_buffer_available() override {
        return true;
    }

    // Returns a stable valid UTC timestamp.
    DiagnosticTime current_time() override {
        return {2026U, 7U, 20U, 1U, 2U, 3U, 4U, 0U};
    }
};

}  // namespace

TEST_CASE(log_007_open_session_uses_4096_buffer_and_flushes_exact_marker) {
    DiagnosticLogWriter writer;
    FakeDiagnosticLogPort port;

    REQUIRE(writer.open_session(100U, port));

    REQUIRE_EQ(port.opened_path, std::string("/sd/.logs/GeneralInfo.log"));
    REQUIRE_EQ(port.requested_buffer_size, 4096U);
    REQUIRE_EQ(text(port.writes[0]),
               std::string("===== log session start [2026-07-20 01:02:03.004] =====\r\n"));
    REQUIRE_EQ(port.events,
               std::vector<std::string>({"open", "write", "flush"}));
}

TEST_CASE(log_007_time_sync_marker_is_written_and_flushed_when_active) {
    DiagnosticLogWriter writer;
    FakeDiagnosticLogPort port;
    writer.open_session(0U, port);
    port.writes.clear();
    port.events.clear();

    writer.system_time_synced(port);

    REQUIRE_EQ(text(port.writes[0]),
               std::string("===== system time synced [2026-07-20 01:02:03.004] =====\r\n"));
    REQUIRE_EQ(port.events, std::vector<std::string>({"write", "flush"}));
}

TEST_CASE(log_008_periodic_flush_requires_more_than_1990_milliseconds) {
    DiagnosticLogWriter writer;
    FakeDiagnosticLogPort port;
    writer.open_session(100U, port);
    port.events.clear();

    writer.poll(2090U, port);
    REQUIRE(port.events.empty());
    writer.poll(2091U, port);
    REQUIRE_EQ(port.events, std::vector<std::string>({"flush"}));
}

TEST_CASE(log_009_rotation_runs_every_best_effort_step_before_opening_new_file) {
    DiagnosticLogWriter writer;
    FakeDiagnosticLogPort port;
    port.open_result = 0x300000U;
    writer.open_session(0U, port);
    port.events.clear();
    port.writes.clear();
    port.open_result = 0U;

    writer.write_record(ByteVector{'x'}, port);

    REQUIRE_EQ(port.events,
               std::vector<std::string>({
                   "close",
                   "remove:/sd/.logs/GeneralInfo.log.2",
                   "rename:/sd/.logs/GeneralInfo.log.1:/sd/.logs/GeneralInfo.log.2",
                   "rename:/sd/.logs/GeneralInfo.log:/sd/.logs/GeneralInfo.log.1",
                   "open",
                   "write",
               }));
}

TEST_CASE(log_010_and_013_rotation_open_failure_stops_logging_and_discards_record) {
    DiagnosticLogWriter writer;
    FakeDiagnosticLogPort port;
    port.open_result = 0x300000U;
    writer.open_session(0U, port);
    port.open_result = std::nullopt;
    port.writes.clear();

    writer.write_record(ByteVector{'x'}, port);

    REQUIRE(!writer.active());
    REQUIRE(port.writes.empty());
}

TEST_CASE(log_013_short_write_advances_only_by_actual_bytes) {
    DiagnosticLogWriter writer;
    FakeDiagnosticLogPort port;
    writer.open_session(0U, port);
    const std::uint64_t before = writer.tracked_size();
    port.next_write_size = 2U;

    writer.write_record(ByteVector{'a', 'b', 'c', 'd'}, port);

    REQUIRE_EQ(writer.tracked_size(), before + 2U);
    REQUIRE(writer.active());
}

TEST_CASE(log_011_shutdown_drains_one_record_per_poll_then_forces_close_at_timeout) {
    DiagnosticCapture capture;
    FakeCapturePort capture_port;
    capture.set_logging_active(true);
    capture.capture("one\n", {}, capture_port);
    capture.capture("two\n", {}, capture_port);
    DiagnosticLogWriter writer;
    FakeDiagnosticLogPort port;
    writer.open_session(0U, port);
    port.events.clear();

    writer.begin_shutdown(100U);
    writer.poll_shutdown(100U, capture, port);
    REQUIRE_EQ(capture.pending_count(), 1U);
    REQUIRE(writer.active());
    writer.poll_shutdown(5100U, capture, port);

    REQUIRE(!writer.active());
    REQUIRE_EQ(port.events[port.events.size() - 2U], std::string("flush"));
    REQUIRE_EQ(port.events.back(), std::string("close"));
}
