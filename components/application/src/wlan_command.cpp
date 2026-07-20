// Implements exact user Wi-Fi scan sequencing and host response framing.
#include "firmware/application/wlan_command.hpp"

#include "firmware/core/protocol_constants.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace firmware::application {
namespace {

constexpr WifiScanConfig user_scan_config{
    true,
    true,
    120U,
    360U,
    20U,
};
constexpr std::uint32_t scan_settle_milliseconds = 100U;
constexpr std::size_t maximum_scan_response_size = 512U;
constexpr std::string_view scanning_message = "正在扫描WiFi网络...\n";
constexpr std::string_view success_message = "ok\r\n";

// Creates a frame from text and the selected packet type.
core::Frame text_frame(std::uint8_t type, std::string_view text) {
    return {type, {text.begin(), text.end()}};
}

}  // namespace

void WlanScanCommand::execute(WlanCommandPort& port) {
    port.send(text_frame(core::protocol::text_response, scanning_message));
    const std::string selected = port.connected_ssid();
    port.stop_scan();
    port.delay_milliseconds(scan_settle_milliseconds);
    const WifiScanOutcome outcome = port.scan(user_scan_config);
    if (!outcome.success) {
        const std::string error = "WiFi scan failed: " + outcome.error_name;
        port.send(text_frame(core::protocol::operation_failure, error));
        return;
    }

    const auto results = core::process_wifi_scan(outcome.observations, selected);
    const std::string formatted = core::format_wifi_scan(results);
    const std::size_t response_size =
        std::min(formatted.size(), maximum_scan_response_size);
    port.send({core::protocol::text_response,
               {formatted.begin(),
                formatted.begin() +
                    static_cast<std::ptrdiff_t>(response_size)}});
    port.send(text_frame(core::protocol::operation_success, success_message));
}

}  // namespace firmware::application
