// Verifies machine-name derivation and congestion-based AP channel selection.
#include "test.hpp"

#include "firmware/core/network_policy.hpp"

#include <array>
#include <string>
#include <vector>

using firmware::core::ByteVector;

TEST_CASE(net_001_machine_name_uses_first_config_value_and_retains_31_bytes) {
    const std::vector<std::string> lines{
        "wifi.machine_name = First",
        "wifi.machine_name=Second",
    };
    const std::array<std::uint8_t, 6U> mac{0U, 1U, 2U, 3U, 0xABU, 0xCDU};

    REQUIRE_EQ(firmware::core::derive_machine_name(lines, mac),
               std::string("First"));

    const std::vector<std::string> long_name{
        "wifi.machine_name=" + std::string(40U, 'x'),
    };
    REQUIRE_EQ(firmware::core::derive_machine_name(long_name, mac).size(), 31U);
}

TEST_CASE(net_002_missing_or_empty_name_uses_final_mac_bytes_in_uppercase) {
    const std::array<std::uint8_t, 6U> mac{0U, 1U, 2U, 3U, 0x0AU, 0xBFU};

    REQUIRE_EQ(firmware::core::derive_machine_name({}, mac),
               std::string("Makera_Z1_0ABF"));
    REQUIRE_EQ(firmware::core::derive_machine_name(
                   std::vector<std::string>{"wifi.machine_name = "}, mac),
               std::string("Makera_Z1_0ABF"));
}

TEST_CASE(net_004_channel_selection_ignores_out_of_range_and_uses_lowest_tie) {
    const std::vector<std::uint8_t> observations{0U, 1U, 1U, 2U, 15U};

    REQUIRE_EQ(firmware::core::select_access_point_channel(observations), 3U);
}

TEST_CASE(net_004_channel_observation_counters_wrap_at_eight_bits) {
    std::vector<std::uint8_t> observations(256U, 1U);
    for (std::uint8_t channel = 2U; channel <= 14U; ++channel) {
        observations.push_back(channel);
    }

    REQUIRE_EQ(firmware::core::select_access_point_channel(observations), 1U);
}

TEST_CASE(net_005_empty_or_unavailable_scan_storage_selects_channel_one) {
    REQUIRE_EQ(firmware::core::select_access_point_channel({}), 1U);
    REQUIRE_EQ(firmware::core::select_access_point_channel({}, false), 1U);
}

TEST_CASE(net_031_scan_uses_only_first_20_observations_and_discards_empty_ssids) {
    std::vector<firmware::core::WifiObservation> observations;
    observations.push_back({{}, -1, 0U});
    for (std::size_t index = 0U; index < 20U; ++index) {
        const std::string name = "network" + std::to_string(index);
        observations.push_back({{name.begin(), name.end()},
                                static_cast<std::int32_t>(-20 - index), 1U});
    }

    const auto results = firmware::core::process_wifi_scan(observations, {});

    REQUIRE_EQ(results.size(), 19U);
    REQUIRE_EQ(results.front().ssid, std::string("network0"));
    REQUIRE_EQ(results.back().ssid, std::string("network18"));
}

TEST_CASE(net_032_short_duplicate_collapses_to_strongest_and_equal_keeps_first) {
    const std::vector<firmware::core::WifiObservation> observations{
        {{'a', 'p'}, -70, 1U},
        {{'a', 'p'}, -40, 2U},
        {{'a', 'p'}, -40, 3U},
    };

    const auto results = firmware::core::process_wifi_scan(observations, {});

    REQUIRE_EQ(results.size(), 1U);
    REQUIRE_EQ(results[0].rssi, -40);
    REQUIRE_EQ(results[0].authentication_mode, 2U);
}

TEST_CASE(net_032_repeated_raw_32_byte_ssid_does_not_match_truncated_result) {
    const ByteVector raw(32U, 'x');
    const std::vector<firmware::core::WifiObservation> observations{
        {raw, -50, 1U},
        {raw, -40, 1U},
    };

    const auto results = firmware::core::process_wifi_scan(observations, {});

    REQUIRE_EQ(results.size(), 2U);
    REQUIRE_EQ(results[0].ssid.size(), 31U);
    REQUIRE_EQ(results[1].ssid.size(), 31U);
}

TEST_CASE(net_032_scan_results_are_stably_sorted_from_strongest_to_weakest) {
    const std::vector<firmware::core::WifiObservation> observations{
        {{'a'}, -50, 0U},
        {{'b'}, -30, 1U},
        {{'c'}, -30, 2U},
    };

    const auto results = firmware::core::process_wifi_scan(observations, {});

    REQUIRE_EQ(results[0].ssid, std::string("b"));
    REQUIRE_EQ(results[1].ssid, std::string("c"));
    REQUIRE_EQ(results[2].ssid, std::string("a"));
}

TEST_CASE(net_033_scan_format_uses_authentication_and_selected_ssid_flags) {
    const std::vector<firmware::core::WifiObservation> observations{
        {{'o', 'p', 'e', 'n'}, -20, 0U},
        {{'s', 'e', 'c', 'u', 'r', 'e'}, -30, 3U},
    };
    const auto results = firmware::core::process_wifi_scan(observations, "secure");

    REQUIRE_EQ(firmware::core::format_wifi_scan(results),
               std::string("open,0,-20,0\nsecure,1,-30,1\n"));
}
