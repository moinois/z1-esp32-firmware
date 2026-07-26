// Declares bounded parsing of host WLAN command arguments.
#pragma once

#include <string>
#include <string_view>

namespace firmware::application {

enum class WlanRequestKind { scan, connect, disconnect, save };

struct WlanRequest {
    WlanRequestKind kind = WlanRequestKind::scan;
    std::string ssid;
    std::string password;
};

// Parses one complete wlan command and applies its exact option precedence.
WlanRequest parse_wlan_request(std::string_view command);

}  // namespace firmware::application
