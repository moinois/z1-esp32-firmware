// Verifies exact expedited SDO client requests and response filtering.
#include "test.hpp"

#include "core/can/canopen_sdo_client.hpp"

using firmware::core::CanFrame;

TEST_CASE(sdo_client_makes_little_endian_upload_request) {
    const auto frame = firmware::core::make_sdo_upload_request(1U, 0x6000U, 1U);
    REQUIRE_EQ(frame.identifier, 0x601U);
    REQUIRE_EQ(frame.size, 8U);
    REQUIRE_EQ(frame.data[0], 0x40U);
    REQUIRE_EQ(frame.data[1], 0x00U);
    REQUIRE_EQ(frame.data[2], 0x60U);
    REQUIRE_EQ(frame.data[3], 1U);
}

TEST_CASE(sdo_client_makes_little_endian_download_request) {
    const auto frame = firmware::core::make_sdo_download_request(
        1U, 0x6001U, 1U, 0xA1B2C3D4U);
    REQUIRE_EQ(frame.data[0], 0x23U);
    REQUIRE_EQ(frame.data[4], 0xD4U);
    REQUIRE_EQ(frame.data[5], 0xC3U);
    REQUIRE_EQ(frame.data[6], 0xB2U);
    REQUIRE_EQ(frame.data[7], 0xA1U);
}

TEST_CASE(sdo_client_parses_matching_value_and_abort) {
    CanFrame response{0x581U, {0x43U, 0x00U, 0x60U, 1U, 0x04U, 0x03U, 0x02U, 0x01U}, 8U};
    const auto value = firmware::core::parse_sdo_client_response(
        response, 1U, 0x6000U, 1U);
    REQUIRE(value.has_value());
    REQUIRE(!value->aborted);
    REQUIRE_EQ(value->value, 0x01020304U);

    response.data[0] = 0x80U;
    const auto abort = firmware::core::parse_sdo_client_response(
        response, 1U, 0x6000U, 1U);
    REQUIRE(abort.has_value());
    REQUIRE(abort->aborted);
}

TEST_CASE(sdo_client_rejects_wrong_object_response) {
    const CanFrame response{0x581U, {0x43U, 0x01U, 0x60U, 1U, 0U, 0U, 0U, 0U}, 8U};
    REQUIRE(!firmware::core::parse_sdo_client_response(
        response, 1U, 0x6000U, 1U));
}
