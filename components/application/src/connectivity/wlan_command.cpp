/** @file @brief Implements exact user Wi-Fi scan sequencing and host response framing. */
#include "firmware/application/wlan_command.hpp"

#include "firmware/application/connectivity_defaults.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace firmware::application {
namespace {

constexpr WifiScanConfig user_scan_config{
    true,
    true,
    connectivity_defaults::user_scan_active_dwell_milliseconds,
    connectivity_defaults::user_scan_passive_dwell_milliseconds,
    connectivity_defaults::user_scan_maximum_observations,
};
constexpr std::size_t maximum_scan_response_size = 512U;
constexpr std::size_t maximum_connection_error_detail_size = 100U;
constexpr std::uint32_t discovery_delay_milliseconds = 500U;
constexpr std::string_view scanning_message = "正在扫描WiFi网络...\n";
constexpr std::string_view success_message = "ok\r\n";
constexpr std::string_view connection_failure_message = "Connect error.\r\n";
constexpr std::string_view disconnection_failure_message =
    "Disconnect error.\r\n";

// Creates a frame from text and the selected packet type.
core::Frame text_frame(std::uint8_t type, std::string_view text) {
    return {type, {text.begin(), text.end()}};
}

}  // namespace

void WlanScanCommand::execute(WlanCommandPort& port) {
    port.send(text_frame(core::protocol::text_response, scanning_message));
    const std::string selected = port.connected_ssid();
    port.stop_scan();
    port.delay_milliseconds(
        connectivity_defaults::user_scan_settle_milliseconds);
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

void WlanConnectionCommand::connect(
    StationRuntime& runtime, StationConnectionPort& station,
    WlanConnectionResponsePort& responses, std::string_view ssid,
    std::string_view password) {
    const ManualConnectionResult result =
        ManualStationConnection::connect(runtime, station, ssid, password);
    if (!result.success) {
        const std::string_view detail = runtime.error_detail.empty()
                                            ? std::string_view(result.error_name)
                                            : std::string_view(runtime.error_detail);
        std::string report = "Error: ";
        report.append(detail.substr(0U, maximum_connection_error_detail_size));
        report.push_back('\n');
        responses.send(text_frame(core::protocol::text_response, report));
        if (detail.rfind("WiFi association failed", 0U) == 0U) {
            std::string failure = report;
            responses.send(text_frame(core::protocol::operation_failure,
                                      failure));
        } else {
            responses.send(text_frame(core::protocol::operation_failure,
                                      connection_failure_message));
        }
        return;
    }

    const std::string report = "WiFi connected, ip: " + result.ipv4 + "\n";
    responses.send(text_frame(core::protocol::text_response, report));
    responses.send(text_frame(core::protocol::operation_success,
                              success_message));
    responses.delay_milliseconds(discovery_delay_milliseconds);
    responses.send_discovery_burst();
}

void WlanConnectionCommand::disconnect(
    StationRuntime& runtime, StationConnectionPort& station,
    WlanConnectionResponsePort& responses) {
    const StationApiResult result =
        ManualStationConnection::disconnect(runtime, station);
    if (!result.success) {
        const std::string report = "Error: " + result.error_name + "\n";
        responses.send(text_frame(core::protocol::text_response, report));
        responses.send(text_frame(core::protocol::operation_failure,
                                  disconnection_failure_message));
        return;
    }

    responses.send(text_frame(core::protocol::text_response,
                              "WiFi Disconnected!\n"));
    responses.send(text_frame(core::protocol::operation_success,
                              success_message));
}

}  // namespace firmware::application
