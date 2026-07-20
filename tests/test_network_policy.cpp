// Verifies machine-name derivation and congestion-based AP channel selection.
#include "test.hpp"

#include "firmware/core/network_policy.hpp"

#include <array>
#include <string>
#include <vector>

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
