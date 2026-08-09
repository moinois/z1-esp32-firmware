// Verifies opt-in serial.log mirroring, its hard size limit, and reactivation.
#include "test.hpp"

#include "application/diagnostics/serial_diagnostic_log.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::SerialDiagnosticLogPort;
using firmware::application::SerialDiagnosticLogWriter;
using firmware::application::serial_diagnostic_log_maximum_size;
using firmware::core::ByteVector;

namespace {

// Records mirror file operations and supplies configurable open/write results.
class FakeSerialLogPort final : public SerialDiagnosticLogPort {
public:
    // Returns the configured existing file size or absence result.
    std::optional<std::uint64_t> open_existing_append(
        std::string_view path) override {
        opened_paths.emplace_back(path);
        return open_result;
    }

    // Records one write and returns its configured or full size.
    std::size_t write(firmware::core::BytesView record) override {
        writes.emplace_back(record.begin(), record.end());
        return write_result.has_value() ? *write_result : record.size();
    }

    // Records one durability flush.
    void flush() override {
        ++flush_count;
    }

    // Records closure of the mirror stream.
    void close() override {
        ++close_count;
    }

    std::optional<std::uint64_t> open_result;
    std::optional<std::size_t> write_result;
    std::vector<std::string> opened_paths;
    std::vector<ByteVector> writes;
    std::size_t flush_count = 0U;
    std::size_t close_count = 0U;
};

}  // namespace

TEST_CASE(serial_log_absence_leaves_optional_mirror_inactive) {
    SerialDiagnosticLogWriter writer;
    FakeSerialLogPort port;

    writer.write_record(ByteVector{'x'}, port);

    REQUIRE(!writer.active());
    REQUIRE_EQ(port.opened_paths,
               std::vector<std::string>({"/sd/serial.log"}));
    REQUIRE(port.writes.empty());
}

TEST_CASE(serial_log_existing_sentinel_appends_and_flushes_each_record) {
    SerialDiagnosticLogWriter writer;
    FakeSerialLogPort port;
    port.open_result = 7U;

    writer.write_record(ByteVector{'a', 'b'}, port);

    REQUIRE(!writer.active());
    REQUIRE_EQ(writer.tracked_size(), 9U);
    REQUIRE_EQ(port.writes, std::vector<ByteVector>({{'a', 'b'}}));
    REQUIRE_EQ(port.flush_count, 1U);
    REQUIRE_EQ(port.close_count, 1U);
}

TEST_CASE(serial_log_never_starts_a_record_beyond_384_kib) {
    SerialDiagnosticLogWriter writer;
    FakeSerialLogPort port;
    port.open_result = serial_diagnostic_log_maximum_size - 1U;

    writer.write_record(ByteVector{'x'}, port);
    port.open_result = serial_diagnostic_log_maximum_size;
    writer.write_record(ByteVector{'y'}, port);

    REQUIRE(!writer.active());
    REQUIRE_EQ(writer.tracked_size(), serial_diagnostic_log_maximum_size);
    REQUIRE_EQ(writer.dropped_record_count(), 1U);
    REQUIRE_EQ(port.writes, std::vector<ByteVector>({{'x'}}));
    REQUIRE_EQ(port.close_count, 2U);
}

TEST_CASE(serial_log_empty_replacement_reactivates_after_size_saturation) {
    SerialDiagnosticLogWriter writer;
    FakeSerialLogPort port;
    port.open_result = serial_diagnostic_log_maximum_size;
    writer.write_record(ByteVector{'x'}, port);
    REQUIRE(!writer.active());

    port.open_result = 0U;
    writer.write_record(ByteVector{'n', 'e', 'w'}, port);

    REQUIRE(!writer.active());
    REQUIRE_EQ(writer.tracked_size(), 3U);
    REQUIRE_EQ(port.writes, std::vector<ByteVector>({{'n', 'e', 'w'}}));
    REQUIRE_EQ(port.close_count, 2U);
}

TEST_CASE(serial_log_short_write_disables_mirror_without_affecting_callers) {
    SerialDiagnosticLogWriter writer;
    FakeSerialLogPort port;
    port.open_result = 0U;
    port.write_result = 1U;

    writer.write_record(ByteVector{'a', 'b'}, port);

    REQUIRE(!writer.active());
    REQUIRE_EQ(writer.tracked_size(), 1U);
    REQUIRE_EQ(writer.dropped_record_count(), 1U);
    REQUIRE_EQ(port.close_count, 1U);
}
