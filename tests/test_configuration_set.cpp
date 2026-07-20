// Verifies SD rewrite mechanics, live updates, and exact config-set replies.
#include "test.hpp"

#include "firmware/application/configuration_set.hpp"
#include "firmware/core/configuration_syntax.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::ConfigurationSet;
using firmware::application::ConfigurationSetPort;
using firmware::application::LiveConfiguration;
using firmware::core::ByteVector;
using firmware::core::Frame;

namespace {

// Converts command and configuration text into byte vectors.
ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

// Converts response payload bytes to text for exact assertions.
std::string text(const ByteVector& value) {
    return {value.begin(), value.end()};
}

// Records live loading and atomic-replacement steps without a filesystem.
class FakeConfigurationSetPort final : public ConfigurationSetPort {
public:
    // Returns configured live chunks.
    std::optional<std::vector<ByteVector>> read_chunks(
        std::string_view, std::size_t) override {
        return live_chunks;
    }

    // Returns the configured active SD text.
    std::optional<std::string> read_active_text(std::string_view path) override {
        read_path = path;
        return active_text;
    }

    // Records the complete temporary file content.
    bool write_temporary(std::string_view path, std::string_view content) override {
        events.emplace_back("write");
        temporary_path = path;
        temporary_text = content;
        return temporary_write_succeeds;
    }

    // Records best-effort unlink of the active file.
    bool unlink_active(std::string_view path) override {
        events.emplace_back("unlink");
        unlink_path = path;
        return unlink_succeeds;
    }

    // Records temporary-to-active rename and returns its configured result.
    bool rename_temporary(std::string_view source,
                          std::string_view destination) override {
        events.emplace_back("rename");
        rename_source = source;
        rename_destination = destination;
        return rename_succeeds;
    }

    // Records cleanup after rename failure.
    void remove_temporary(std::string_view path) override {
        events.emplace_back("remove_temp");
        removed_path = path;
    }

    // Records one response frame.
    void send(Frame frame) override {
        sent.push_back(std::move(frame));
    }

    std::optional<std::vector<ByteVector>> live_chunks =
        std::vector<ByteVector>{};
    std::optional<std::string> active_text = std::string{};
    bool temporary_write_succeeds = true;
    bool unlink_succeeds = true;
    bool rename_succeeds = true;
    std::string read_path;
    std::string temporary_path;
    std::string temporary_text;
    std::string unlink_path;
    std::string rename_source;
    std::string rename_destination;
    std::string removed_path;
    std::vector<std::string> events;
    std::vector<Frame> sent;
};

}  // namespace

TEST_CASE(cfg_030_sd_rewrite_replaces_every_match_and_preserves_delimiter_suffix) {
    const std::string rewritten = firmware::core::rewrite_sd_configuration(
        " key = old  # note\r\nkey second\t#two\nother=x\n", "key", "new");

    REQUIRE_EQ(rewritten,
               std::string("key=new  # note\nkey new\t#two\nother=x\n"));
}

TEST_CASE(cfg_030_missing_sd_key_appends_after_a_required_line_boundary) {
    REQUIRE_EQ(firmware::core::rewrite_sd_configuration("other=x", "key", "value"),
               std::string("other=x\nkey value\n"));
}

TEST_CASE(cfg_031_sd_set_writes_unlinks_and_renames_even_if_unlink_fails) {
    LiveConfiguration live;
    FakeConfigurationSetPort port;
    port.active_text = std::string("key=old\n");
    port.unlink_succeeds = false;

    ConfigurationSet::execute(bytes(" sd key new"), live, port);

    REQUIRE_EQ(port.read_path, std::string("/sd/config.txt"));
    REQUIRE_EQ(port.temporary_path, std::string("/sd/config.tmp"));
    REQUIRE_EQ(port.temporary_text, std::string("key=new\n"));
    REQUIRE_EQ(port.events,
               std::vector<std::string>({"write", "unlink", "rename"}));
    REQUIRE_EQ(port.rename_destination, std::string("/sd/config.txt"));
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("sd: key has been set to new\r\n"));
}

TEST_CASE(cfg_031_rename_failure_removes_temporary_and_reports_set_failure) {
    LiveConfiguration live;
    FakeConfigurationSetPort port;
    port.rename_succeeds = false;

    ConfigurationSet::execute(bytes(" sd key value"), live, port);

    REQUIRE_EQ(port.events,
               std::vector<std::string>({"write", "unlink", "rename", "remove_temp"}));
    REQUIRE_EQ(port.removed_path, std::string("/sd/config.tmp"));
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("sd: key not enough space to overwrite existing key/value\r\n"));
}

TEST_CASE(cfg_032_missing_source_key_or_value_sends_exact_usage) {
    LiveConfiguration live;
    FakeConfigurationSetPort port;

    ConfigurationSet::execute(bytes(" sd key"), live, port);

    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("Usage: config-set source setting value # where source is sd, setting is the key and value is the new value\r\n"));

    FakeConfigurationSetPort empty_key;
    const ByteVector nul_key{' ', 's', 'd', ' ', 0U, 'x', ' ', 'v'};
    ConfigurationSet::execute(nul_key, live, empty_key);
    REQUIRE_EQ(text(empty_key.sent[0].payload),
               std::string("Usage: config-set source setting value # where source is sd, setting is the key and value is the new value\r\n"));
}

TEST_CASE(cfg_033_unknown_source_sends_the_source_specific_error) {
    LiveConfiguration live;
    FakeConfigurationSetPort port;

    ConfigurationSet::execute(bytes(" cloud key value"), live, port);

    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("cloud source does not exist\r\n"));
}

TEST_CASE(cfg_023_and_034_live_set_reports_requested_text_despite_truncation) {
    LiveConfiguration live;
    FakeConfigurationSetPort port;
    const std::string key(70U, 'k');
    const std::string value(300U, 'v');
    const std::string command = " live " + key + " " + value;

    ConfigurationSet::execute(bytes(command), live, port);

    REQUIRE_EQ(live.entries().front().key.size(), 63U);
    REQUIRE_EQ(live.entries().front().value.size(), 255U);
    REQUIRE_EQ(text(port.sent[0].payload),
               "live: " + key + " has been set to " + value + "\r\n");
}

TEST_CASE(cfg_034_full_live_view_reports_the_exact_capacity_failure) {
    LiveConfiguration live;
    FakeConfigurationSetPort port;
    live.ensure_loaded(port);
    for (std::size_t index = 0U; index < 100U; ++index) {
        REQUIRE(live.set("key" + std::to_string(index), "value"));
    }

    ConfigurationSet::execute(bytes(" live overflow value"), live, port);

    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("live: overflow not enough space to overwrite existing key/value\r\n"));
}
