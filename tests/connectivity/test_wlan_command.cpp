// Verifies wlan parsing and the host-visible user scan exchange.
#include "test.hpp"

#include "application/connectivity/wlan_command.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::WifiScanConfig;
using firmware::application::WifiScanOutcome;
using firmware::application::WlanCommandPort;
using firmware::application::WlanScanCommand;
using firmware::core::ByteVector;
using firmware::core::Frame;
using firmware::core::WlanAction;

namespace {

// Converts command text to protocol bytes.
ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

// Converts response bytes to text for exact assertions.
std::string text(const ByteVector& value) {
    return {value.begin(), value.end()};
}

// Records scan sequencing and supplies configured observations or failure.
class FakeWlanCommandPort final : public WlanCommandPort {
public:
    // Records cancellation of a prior scan.
    void stop_scan() override {
        events.emplace_back("stop");
    }

    // Records the pre-scan settling delay.
    void delay_milliseconds(std::uint32_t duration) override {
        events.emplace_back("delay");
        delay = duration;
    }

    // Returns the configured scan result and records exact settings.
    WifiScanOutcome scan(const WifiScanConfig& requested) override {
        events.emplace_back("scan");
        config = requested;
        return outcome;
    }

    // Returns the SSID connected when scanning began.
    std::string connected_ssid() const override {
        return selected;
    }

    // Records one host response frame.
    void send(Frame frame) override {
        sent.push_back(std::move(frame));
    }

    WifiScanOutcome outcome{true, {}, {}};
    WifiScanConfig config{};
    std::string selected;
    std::uint32_t delay = 0U;
    std::vector<std::string> events;
    std::vector<Frame> sent;
};

}  // namespace

TEST_CASE(net_040_bare_malformed_or_missing_ssid_wlan_selects_scan) {
    REQUIRE_EQ(firmware::core::parse_wlan_command(bytes("wlan")).action,
               WlanAction::scan);
    REQUIRE_EQ(firmware::core::parse_wlan_command(bytes("wlan x")).action,
               WlanAction::connect);
    REQUIRE_EQ(firmware::core::parse_wlan_command(bytes("wlan  ")).action,
               WlanAction::scan);
    REQUIRE_EQ(firmware::core::parse_wlan_command(ByteVector(129U, 'x')).action,
               WlanAction::scan);
}

TEST_CASE(net_041_wlan_checks_all_options_and_retains_only_first_two_values) {
    const auto parsed = firmware::core::parse_wlan_command(
        bytes("wlan -e my\x01ssid password ignored -d"));

    REQUIRE_EQ(parsed.action, WlanAction::disconnect);
    REQUIRE_EQ(parsed.ssid, std::string("my ssid"));
    REQUIRE_EQ(parsed.password, std::string("password"));
}

TEST_CASE(net_030_and_042_scan_stops_waits_and_uses_exact_active_settings) {
    FakeWlanCommandPort port;

    WlanScanCommand::execute(port);

    REQUIRE_EQ(port.events,
               std::vector<std::string>({"stop", "delay", "scan"}));
    REQUIRE_EQ(port.delay, 100U);
    REQUIRE(port.config.active);
    REQUIRE(port.config.include_hidden);
    REQUIRE_EQ(port.config.active_dwell_milliseconds, 120U);
    REQUIRE_EQ(port.config.passive_dwell_milliseconds, 360U);
    REQUIRE_EQ(port.config.maximum_observations, 20U);
    REQUIRE_EQ(port.sent[0].type, 0x83U);
    REQUIRE_EQ(text(port.sent[0].payload), std::string("正在扫描WiFi网络...\n"));
}

TEST_CASE(net_042_scan_failure_sends_exact_error_without_line_ending) {
    FakeWlanCommandPort port;
    port.outcome = {false, "ESP_FAIL", {}};

    WlanScanCommand::execute(port);

    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent[1].type, 0x85U);
    REQUIRE_EQ(text(port.sent[1].payload),
               std::string("WiFi scan failed: ESP_FAIL"));
}

TEST_CASE(net_043_scan_success_sends_at_most_512_bytes_then_ok) {
    FakeWlanCommandPort port;
    for (std::size_t index = 0U; index < 20U; ++index) {
        const std::string ssid(31U, static_cast<char>('a' + index));
        port.outcome.observations.push_back(
            {{ssid.begin(), ssid.end()}, -static_cast<std::int32_t>(index), 1U});
    }

    WlanScanCommand::execute(port);

    REQUIRE_EQ(port.sent.size(), 3U);
    REQUIRE_EQ(port.sent[1].type, 0x83U);
    REQUIRE_EQ(port.sent[1].payload.size(), 512U);
    REQUIRE_EQ(port.sent[2].type, 0x84U);
    REQUIRE_EQ(text(port.sent[2].payload), std::string("ok\r\n"));
}

TEST_CASE(net_043_empty_scan_still_sends_an_empty_text_frame_then_ok) {
    FakeWlanCommandPort port;

    WlanScanCommand::execute(port);

    REQUIRE_EQ(port.sent.size(), 3U);
    REQUIRE(port.sent[1].payload.empty());
}
