// Verifies configuration restore/default copying and exact result responses.
#include "test.hpp"

#include "firmware/application/configuration_files.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::ByteRead;
using firmware::application::ByteReadStatus;
using firmware::application::ConfigurationFilePort;
using firmware::application::ConfigurationFiles;
using firmware::core::Frame;

namespace {

// Converts a response payload to text for exact assertions.
std::string text(const firmware::core::ByteVector& value) {
    return {value.begin(), value.end()};
}

// Emulates bytewise source and destination operations in memory.
class FakeConfigurationFilePort final : public ConfigurationFilePort {
public:
    std::string_view active_configuration_path() const override {
        return "/sd/config.txt";
    }
    std::string_view default_configuration_path() const override {
        return "/sd/config.default";
    }
    // Reports configured source presence and records its path.
    bool file_exists(std::string_view path) override {
        existence_path = path;
        return source_exists;
    }

    // Opens the configured source byte stream.
    bool open_source(std::string_view path) override {
        source_path = path;
        source_cursor = 0U;
        return source_opens;
    }

    // Truncates or creates the in-memory destination.
    bool open_truncated_destination(std::string_view path) override {
        destination_path = path;
        destination.clear();
        return destination_opens;
    }

    // Reads one source byte, EOF, or the configured failure.
    ByteRead read_byte() override {
        if (read_fails_at == source_cursor) {
            return {ByteReadStatus::failure, 0U};
        }
        if (source_cursor == source.size()) {
            return {ByteReadStatus::end_of_file, 0U};
        }
        return {ByteReadStatus::byte, source[source_cursor++]};
    }

    // Appends one byte unless the configured write point fails.
    bool write_byte(std::uint8_t value) override {
        if (destination.size() == write_fails_at) {
            return false;
        }
        destination.push_back(value);
        return true;
    }

    // Records source closure after every opened copy attempt.
    void close_source() override {
        ++source_close_count;
    }

    // Reports the configured destination close result.
    bool close_destination() override {
        ++destination_close_count;
        return destination_close_succeeds;
    }

    // Records one response frame.
    void send(Frame frame) override {
        sent.push_back(std::move(frame));
    }

    bool source_exists = true;
    bool source_opens = true;
    bool destination_opens = true;
    bool destination_close_succeeds = true;
    std::size_t read_fails_at = static_cast<std::size_t>(-1);
    std::size_t write_fails_at = static_cast<std::size_t>(-1);
    std::vector<std::uint8_t> source{'a', 'b', 'c'};
    std::vector<std::uint8_t> destination;
    std::size_t source_cursor = 0U;
    std::size_t source_close_count = 0U;
    std::size_t destination_close_count = 0U;
    std::string existence_path;
    std::string source_path;
    std::string destination_path;
    std::vector<Frame> sent;
};

}  // namespace

TEST_CASE(cfg_004_restore_reports_missing_default_without_opening_destination) {
    FakeConfigurationFilePort port;
    port.source_exists = false;

    ConfigurationFiles::restore(port);

    REQUIRE_EQ(port.existence_path, std::string("/sd/config.default"));
    REQUIRE(port.destination_path.empty());
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("Default file not found: /sd/config.default\r\n"));
}

TEST_CASE(cfg_005_save_default_reports_missing_active_configuration) {
    FakeConfigurationFilePort port;
    port.source_exists = false;

    ConfigurationFiles::save_default(port);

    REQUIRE_EQ(port.existence_path, std::string("/sd/config.txt"));
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("Config file not found: /sd/config.txt\r\n"));
}

TEST_CASE(cfg_004_to_006_restore_truncates_and_copies_every_byte_before_success) {
    FakeConfigurationFilePort port;
    port.destination = {'o', 'l', 'd'};

    ConfigurationFiles::restore(port);

    REQUIRE_EQ(port.source_path, std::string("/sd/config.default"));
    REQUIRE_EQ(port.destination_path, std::string("/sd/config.txt"));
    REQUIRE_EQ(port.destination, std::vector<std::uint8_t>({'a', 'b', 'c'}));
    REQUIRE_EQ(port.source_close_count, 1U);
    REQUIRE_EQ(port.destination_close_count, 1U);
    REQUIRE_EQ(port.sent[0].type, 0x90U);
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("Settings restored complete.\r\n"));
}

TEST_CASE(cfg_005_and_006_default_save_reverses_paths_and_reports_success) {
    FakeConfigurationFilePort port;

    ConfigurationFiles::save_default(port);

    REQUIRE_EQ(port.source_path, std::string("/sd/config.txt"));
    REQUIRE_EQ(port.destination_path, std::string("/sd/config.default"));
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("Settings save as default complete.\r\n"));
}

TEST_CASE(cfg_006_read_write_and_close_failures_leave_partial_destination) {
    FakeConfigurationFilePort read_failure;
    read_failure.read_fails_at = 1U;
    ConfigurationFiles::restore(read_failure);
    REQUIRE_EQ(read_failure.destination, std::vector<std::uint8_t>({'a'}));
    REQUIRE_EQ(text(read_failure.sent[0].payload),
               std::string("Config file not found or created fail: /sd/config.txt\r\n"));

    FakeConfigurationFilePort write_failure;
    write_failure.write_fails_at = 2U;
    ConfigurationFiles::save_default(write_failure);
    REQUIRE_EQ(write_failure.destination,
               std::vector<std::uint8_t>({'a', 'b'}));
    REQUIRE_EQ(text(write_failure.sent[0].payload),
               std::string("Default file not found or created fail: /sd/config.default\r\n"));

    FakeConfigurationFilePort close_failure;
    close_failure.destination_close_succeeds = false;
    ConfigurationFiles::restore(close_failure);
    REQUIRE_EQ(close_failure.destination,
               std::vector<std::uint8_t>({'a', 'b', 'c'}));
    REQUIRE_EQ(text(close_failure.sent[0].payload),
               std::string("Config file not found or created fail: /sd/config.txt\r\n"));
}
