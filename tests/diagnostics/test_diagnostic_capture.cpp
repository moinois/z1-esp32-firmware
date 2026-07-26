// Verifies bounded diagnostic printing, queueing, ANSI removal, and timestamps.
#include "test.hpp"

#include "firmware/application/diagnostic_capture.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::CaptureContext;
using firmware::application::DiagnosticCapture;
using firmware::application::DiagnosticCapturePort;
using firmware::application::DiagnosticTime;
using firmware::core::ByteVector;

namespace {

// Converts retained bytes to text for exact assertions.
std::string text(const ByteVector& value) {
    return {value.begin(), value.end()};
}

// Records prior-output writes and supplies deterministic buffer and clock state.
class FakeDiagnosticCapturePort final : public DiagnosticCapturePort {
public:
    // Records the bytes sent to the prior diagnostic destination.
    void print(firmware::core::BytesView message) override {
        printed.emplace_back(message.begin(), message.end());
    }

    // Reports whether one SD record buffer is available.
    bool record_buffer_available() override {
        return buffer_available;
    }

    // Returns the configured wall-clock and uptime snapshot.
    DiagnosticTime current_time() override {
        return time;
    }

    bool buffer_available = true;
    DiagnosticTime time{2026U, 7U, 20U, 1U, 2U, 3U, 4U, 1234U};
    std::vector<ByteVector> printed;
};

}  // namespace

TEST_CASE(log_001_and_002_capture_always_prints_at_most_511_and_returns_full_length) {
    DiagnosticCapture capture;
    FakeDiagnosticCapturePort port;
    const std::string message(600U, 'x');

    const std::size_t length = capture.capture(message, {}, port);

    REQUIRE_EQ(length, 600U);
    REQUIRE_EQ(port.printed.size(), 1U);
    REQUIRE_EQ(port.printed[0].size(), 511U);
    REQUIRE_EQ(capture.pending_count(), 0U);
}

TEST_CASE(log_001_and_005_active_capture_removes_ansi_and_prepends_timestamp) {
    DiagnosticCapture capture;
    FakeDiagnosticCapturePort port;
    capture.set_logging_active(true);

    capture.capture("plain \x1b[31mred\x1b[0m\r\n", {}, port);
    const auto record = capture.take_pending();

    REQUIRE(record.has_value());
    REQUIRE_EQ(text(*record),
               std::string("[2026-07-20 01:02:03.004] plain red\r\n"));
}

TEST_CASE(log_003_queue_holds_48_records_and_buffer_exhaustion_does_not_block_printing) {
    DiagnosticCapture capture;
    FakeDiagnosticCapturePort port;
    capture.set_logging_active(true);
    for (std::size_t index = 0U; index < 49U; ++index) {
        capture.capture("line\n", {}, port);
    }
    REQUIRE_EQ(capture.pending_count(), 48U);
    REQUIRE_EQ(port.printed.size(), 49U);

    DiagnosticCapture unavailable;
    unavailable.set_logging_active(true);
    port.buffer_available = false;
    unavailable.capture("line\n", {}, port);
    REQUIRE_EQ(unavailable.pending_count(), 0U);
    REQUIRE_EQ(port.printed.size(), 50U);
}

TEST_CASE(log_004_recursive_or_low_stack_capture_bypasses_sd_copying) {
    DiagnosticCapture capture;
    FakeDiagnosticCapturePort port;
    capture.set_logging_active(true);

    capture.capture("recursive\n", CaptureContext{true, false}, port);
    capture.capture("low stack\n", CaptureContext{false, true}, port);

    REQUIRE_EQ(capture.pending_count(), 0U);
    REQUIRE_EQ(port.printed.size(), 2U);
}

TEST_CASE(log_006_pre_2020_clock_uses_uptime_timestamp) {
    DiagnosticCapture capture;
    FakeDiagnosticCapturePort port;
    port.time.year = 2019U;
    port.time.uptime_milliseconds = 9876U;
    capture.set_logging_active(true);

    capture.capture("line\n", {}, port);

    REQUIRE_EQ(text(*capture.take_pending()),
               std::string("[uptime 9876 ms] line\n"));
}
