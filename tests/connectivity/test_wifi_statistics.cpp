// Verifies stable and safely escaped Wi-Fi diagnostic JSON.
#include "test.hpp"

#include "firmware/core/wifi_statistics.hpp"

TEST_CASE(wifi_diagnostics_formats_current_state_counters_and_events) {
    firmware::core::WifiStatistics statistics;
    statistics.connected = true;
    statistics.rssi_dbm = -48;
    statistics.channel = 10U;
    statistics.authentication = "WPA2-PSK";
    statistics.ipv4_address = "192.168.8.119";
    statistics.station_starts = 1U;
    statistics.associations = 3U;
    statistics.disconnections = 2U;
    statistics.addresses_acquired = 3U;
    statistics.addresses_lost = 1U;
    statistics.last_disconnect_reason = 205U;
    statistics.reset_reason = 8U;
    statistics.recent_events = "connected\nreason=\"205\"\\end";

    REQUIRE_EQ(firmware::core::format_wifi_statistics_json(statistics),
               "{\"connected\":true,\"rssi_dbm\":-48,\"channel\":10,"
               "\"authentication\":\"WPA2-PSK\","
               "\"ipv4_address\":\"192.168.8.119\","
               "\"station_starts\":1,\"associations\":3,"
               "\"disconnections\":2,\"addresses_acquired\":3,"
               "\"addresses_lost\":1,\"last_disconnect_reason\":205,"
               "\"reset_reason\":8,"
               "\"recent_events\":\"connected\\nreason=\\\"205\\\"\\\\end\"}");
}
