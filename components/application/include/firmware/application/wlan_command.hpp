// Declares the host WLAN scan exchange behind a replaceable Wi-Fi port.
#pragma once

#include "firmware/core/frame.hpp"
#include "firmware/core/network_policy.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace firmware::application {

// Holds the exact blocking user-scan parameters passed to the target adapter.
struct WifiScanConfig {
    bool active = false;
    bool include_hidden = false;
    std::uint16_t active_dwell_milliseconds = 0U;
    std::uint16_t passive_dwell_milliseconds = 0U;
    std::size_t maximum_observations = 0U;
};

// Holds either scan observations or the target error name.
struct WifiScanOutcome {
    bool success;
    std::string error_name;
    std::vector<core::WifiObservation> observations;
};

// Isolates scan command policy from delays, Wi-Fi APIs, and host transport.
class WlanCommandPort {
public:
    // Enables safe destruction through a substituted port implementation.
    virtual ~WlanCommandPort() = default;

    // Stops any scan that may already be active.
    virtual void stop_scan() = 0;

    // Performs the required settling delay before starting the new scan.
    virtual void delay_milliseconds(std::uint32_t duration) = 0;

    // Performs one blocking scan with exact product parameters.
    virtual WifiScanOutcome scan(const WifiScanConfig& config) = 0;

    // Returns the SSID that was connected when the scan began.
    virtual std::string connected_ssid() const = 0;

    // Sends one response to the requesting host.
    virtual void send(core::Frame frame) = 0;
};

// Executes the scan branch of a host WLAN command.
class WlanScanCommand {
public:
    // Sends progress, performs the blocking scan, and maps its result.
    static void execute(WlanCommandPort& port);
};

}  // namespace firmware::application
