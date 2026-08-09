// Verifies the byte-exact BLE-002 name and BWF-002 conditional AD layout.
#include "test.hpp"

#include "application/provisioning/blufi_advertising.hpp"

#include <string>

using firmware::core::ByteVector;

TEST_CASE(ble_002_name_prefixes_and_truncates_machine_name_by_bytes) {
    REQUIRE_EQ(firmware::application::blufi_device_name(""), std::string("MK_"));
    REQUIRE_EQ(firmware::application::blufi_device_name("Makera_Z1_XXXX"),
               std::string("MK_Makera_Z1_XXXX"));
    REQUIRE_EQ(firmware::application::blufi_device_name(std::string(30U, 'x')),
               std::string("MK_") + std::string(23U, 'x'));
}

TEST_CASE(bwf_002_short_name_includes_all_conditional_structures_in_order) {
    REQUIRE_EQ(firmware::application::blufi_advertising_data("MK_abc", 3),
               ByteVector({0x02U, 0x01U, 0x06U,
                           0x07U, 0x09U, 'M', 'K', '_', 'a', 'b', 'c',
                           0x02U, 0x0AU, 0x03U,
                           0x03U, 0x03U, 0xFFU, 0xFFU,
                           0x05U, 0x12U, 0x06U, 0x00U, 0x10U, 0x00U}));
}

TEST_CASE(bwf_002_suffix_boundaries_pack_exactly_within_legacy_capacity) {
    const auto ten = firmware::application::blufi_advertising_data(
        std::string("MK_") + std::string(10U, 'z'), -6);
    REQUIRE_EQ(ByteVector(ten.end() - 6, ten.end()),
               ByteVector({0x05U, 0x12U, 0x06U, 0x00U, 0x10U, 0x00U}));

    const auto eleven = firmware::application::blufi_advertising_data(
        std::string("MK_") + std::string(11U, 'z'), -6);
    REQUIRE_EQ(eleven.size(), 26U);
    REQUIRE_EQ(ByteVector(eleven.end() - 4, eleven.end()),
               ByteVector({0x03U, 0x03U, 0xFFU, 0xFFU}));

    const auto sixteen = firmware::application::blufi_advertising_data(
        std::string("MK_") + std::string(16U, 'a'), -3);
    REQUIRE_EQ(sixteen.size(), 31U);
    REQUIRE_EQ(ByteVector(sixteen.end() - 4, sixteen.end()),
               ByteVector({0x03U, 0x03U, 0xFFU, 0xFFU}));

    const auto seventeen = firmware::application::blufi_advertising_data(
        std::string("MK_") + std::string(17U, 'b'), 0);
    REQUIRE_EQ(seventeen.size(), 30U);
    REQUIRE_EQ(ByteVector(seventeen.end() - 2, seventeen.end()),
               ByteVector({0x01U, 0x02U}));

    const auto twenty = firmware::application::blufi_advertising_data(
        std::string("MK_") + std::string(20U, 'c'), 6);
    REQUIRE_EQ(twenty.size(), 31U);
    REQUIRE_EQ(ByteVector(twenty.end() - 3, twenty.end()),
               ByteVector({0x02U, 0x0AU, 0x06U}));

    const auto twenty_one = firmware::application::blufi_advertising_data(
        std::string("MK_") + std::string(21U, 'd'), 9);
    REQUIRE_EQ(twenty_one.size(), 29U);
    REQUIRE_EQ(twenty_one[3], 25U);

    const auto twenty_three = firmware::application::blufi_advertising_data(
        std::string("MK_") + std::string(30U, 'e'), 9);
    REQUIRE_EQ(twenty_three.size(), 31U);
    REQUIRE_EQ(twenty_three[3], 27U);
}
