// Verifies cached, SD, live, and unknown config-get source behavior.
#include "test.hpp"

#include "application/configuration/configuration_get.hpp"
#include "application/configuration/configuration_tags.hpp"
#include "application/camera/camera_settings.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::ConfigurationGet;
using firmware::application::ConfigurationGetPort;
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

// Supplies independent live chunks and fresh SD lines for get operations.
class FakeConfigurationGetPort final : public ConfigurationGetPort {
public:
    // Returns configured live chunks and records each lazy load.
    std::optional<std::vector<ByteVector>> read_configuration_chunks(
        std::size_t) override {
        ++live_read_count;
        return live_chunks;
    }

    // Returns configured fresh SD lines and records each direct read.
    std::optional<std::string> read_value(std::string_view tag,
                                          std::string_view key) override {
        ++sd_read_count;
        if (tag == firmware::application::camera_configuration_tag &&
            key == "key") {
            return sd_value;
        }
        if (tag != firmware::application::mainboard_configuration_tag) {
            return std::nullopt;
        }
        return sd_value;
    }

    // Records one response frame.
    void send(Frame frame) override {
        sent.push_back(std::move(frame));
    }

    std::optional<std::vector<ByteVector>> live_chunks =
        std::vector<ByteVector>{};
    std::optional<std::string> sd_value = std::string("sd-value");
    std::size_t live_read_count = 0U;
    std::size_t sd_read_count = 0U;
    std::vector<Frame> sent;
};

}  // namespace

TEST_CASE(cfg_011_cached_get_discards_existing_live_view_before_and_after_reply) {
    LiveConfiguration live;
    FakeConfigurationGetPort port;
    port.live_chunks = std::vector<ByteVector>{bytes("key=old")};
    live.ensure_loaded(port);
    port.live_chunks = std::vector<ByteVector>{bytes("key=fresh")};

    ConfigurationGet::execute(bytes(" key"), live, port);

    REQUIRE_EQ(port.live_read_count, 2U);
    REQUIRE_EQ(port.sent[0].type, 0x83U);
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("cached: key is set to fresh\r\n"));
    port.live_chunks = std::vector<ByteVector>{bytes("key=after")};
    live.ensure_loaded(port);
    REQUIRE_EQ(port.live_read_count, 3U);
    REQUIRE_EQ(live.find("key"), std::optional<std::string>("after"));
}

TEST_CASE(cfg_011_cached_get_reports_a_missing_key) {
    LiveConfiguration live;
    FakeConfigurationGetPort port;

    ConfigurationGet::execute(bytes(" absent"), live, port);

    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("cached: absent is not in config\r\n"));
}

TEST_CASE(cfg_012_sd_get_parses_the_file_afresh_and_uses_console_response) {
    LiveConfiguration live;
    FakeConfigurationGetPort port;
    port.sd_value = std::string("sd-value");

    ConfigurationGet::execute(bytes(" sd key"), live, port);

    REQUIRE_EQ(port.sd_read_count, 1U);
    REQUIRE_EQ(port.sent[0].type, 0x90U);
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("sd: key is set to sd-value\r\n"));
}

TEST_CASE(cfg_012_sd_get_forwards_tag_and_key_to_store) {
    LiveConfiguration live;
    FakeConfigurationGetPort port;

    const std::string command =
        std::string(" sd ") +
        std::string(firmware::application::camera_configuration_tag) +
        " key";
    ConfigurationGet::execute(bytes(command), live, port);

    REQUIRE_EQ(port.sd_read_count, 1U);
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("sd: key is set to sd-value\r\n"));
}

TEST_CASE(cfg_013_live_get_loads_once_and_retains_the_snapshot) {
    LiveConfiguration live;
    FakeConfigurationGetPort port;
    port.live_chunks = std::vector<ByteVector>{bytes("key=first")};

    ConfigurationGet::execute(bytes(" live key"), live, port);
    port.live_chunks = std::vector<ByteVector>{bytes("key=changed")};
    ConfigurationGet::execute(bytes(" live key"), live, port);

    REQUIRE_EQ(port.live_read_count, 1U);
    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(text(port.sent[1].payload),
               std::string("live: key is set to first\r\n"));
}

TEST_CASE(cfg_011_and_013_unknown_source_is_silent_but_one_token_is_a_cached_key) {
    LiveConfiguration live;
    FakeConfigurationGetPort port;

    ConfigurationGet::execute(bytes(" unknown key"), live, port);
    REQUIRE(port.sent.empty());

    ConfigurationGet::execute(bytes(" live"), live, port);

    REQUIRE_EQ(port.sent.size(), 1U);
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("cached: live is not in config\r\n"));
}
