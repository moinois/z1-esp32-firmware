// Verifies WLAN command shape, option precedence, and escaped tokens.
#include "test.hpp"
#include "application/connectivity/wlan_request.hpp"

TEST_CASE(wlan_request_defaults_to_scan) {
    REQUIRE_EQ(firmware::application::parse_wlan_request("wlan").kind,
               firmware::application::WlanRequestKind::scan);
    REQUIRE_EQ(firmware::application::parse_wlan_request("wlan -e only").kind,
               firmware::application::WlanRequestKind::scan);
}

TEST_CASE(wlan_request_parses_connect_and_disconnect_precedence) {
    const auto connect = firmware::application::parse_wlan_request(
        "wlan My\x01SSID secret -e");
    REQUIRE_EQ(connect.kind, firmware::application::WlanRequestKind::connect);
    REQUIRE_EQ(connect.ssid, "My SSID");
    REQUIRE_EQ(connect.password, "secret");
    REQUIRE_EQ(firmware::application::parse_wlan_request(
                   "wlan ssid password -d").kind,
               firmware::application::WlanRequestKind::disconnect);
}

TEST_CASE(wlan_request_parses_save_credentials) {
    const auto save = firmware::application::parse_wlan_request(
        "wlan -s Away SailWithMe");
    REQUIRE_EQ(save.kind, firmware::application::WlanRequestKind::save);
    REQUIRE_EQ(save.ssid, "Away");
    REQUIRE_EQ(save.password, "SailWithMe");
}
