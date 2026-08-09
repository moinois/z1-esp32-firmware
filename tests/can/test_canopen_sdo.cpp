// Tests byte-exact expedited CANopen SDO server requests and responses.
#include "test.hpp"

#include "core/can/canopen_sdo.hpp"

using firmware::core::CanFrame;
using firmware::core::CanopenObjectDictionary;
using firmware::core::CanopenSdoServer;

namespace {

// Creates one full-size request on the local SDO server identifier.
CanFrame request(std::uint8_t command,
                 std::uint16_t index,
                 std::uint8_t subindex) {
    CanFrame frame;
    frame.identifier = firmware::core::canopen::sdo_request_identifier;
    frame.size = 8U;
    frame.data[0] = command;
    frame.data[1] = static_cast<std::uint8_t>(index);
    frame.data[2] = static_cast<std::uint8_t>(index >> 8U);
    frame.data[3] = subindex;
    return frame;
}

// Decodes the four-byte little-endian abort field in an SDO response.
std::uint32_t abort_code(const CanFrame& frame) {
    return static_cast<std::uint32_t>(frame.data[4]) |
           (static_cast<std::uint32_t>(frame.data[5]) << 8U) |
           (static_cast<std::uint32_t>(frame.data[6]) << 16U) |
           (static_cast<std::uint32_t>(frame.data[7]) << 24U);
}

}  // namespace

TEST_CASE(can_002_sdo_server_constants_disable_blocks_and_set_timeout) {
    REQUIRE_EQ(firmware::core::canopen::sdo_server_timeout_milliseconds,
               2000U);
    REQUIRE_EQ(firmware::core::canopen::sdo_client_timeout_milliseconds,
               500U);
    REQUIRE(!firmware::core::canopen::block_transfer_enabled);
}

TEST_CASE(od_003_expedited_upload_encodes_one_two_and_four_byte_values) {
    CanopenObjectDictionary dictionary;
    CanopenSdoServer server(dictionary);

    const auto one = server.handle(request(0x40U, 0x1001U, 0U));
    REQUIRE(one.has_value());
    REQUIRE_EQ(one->frame.data[0], 0x4fU);
    REQUIRE_EQ(one->frame.data[4], 0U);

    const auto two = server.handle(request(0x40U, 0x1017U, 0U));
    REQUIRE_EQ(two->frame.data[0], 0x4bU);

    const auto four = server.handle(request(0x40U, 0x1005U, 0U));
    REQUIRE_EQ(four->frame.data[0], 0x43U);
    REQUIRE_EQ(four->frame.data[4], 0x80U);
    REQUIRE_EQ(four->frame.identifier,
               firmware::core::canopen::sdo_response_identifier);
    REQUIRE_EQ(four->frame.size, 8U);
}

TEST_CASE(od_003_expedited_download_writes_and_returns_dictionary_effects) {
    CanopenObjectDictionary dictionary;
    CanopenSdoServer server(dictionary);
    CanFrame write = request(0x2bU, 0x1017U, 0U);
    write.data[4] = 0xfaU;
    write.data[5] = 0U;

    const auto result = server.handle(write);

    REQUIRE(result.has_value());
    REQUIRE_EQ(result->frame.data[0], 0x60U);
    REQUIRE_EQ(result->frame.data[1], 0x17U);
    REQUIRE_EQ(result->frame.data[2], 0x10U);
    REQUIRE_EQ(result->effects.producer_heartbeat_period, 250U);
    REQUIRE_EQ(dictionary.read(0x1017U, 0U).data[0], 0xfaU);
}

TEST_CASE(od_003_dictionary_abort_is_encoded_little_endian) {
    CanopenObjectDictionary dictionary;
    CanopenSdoServer server(dictionary);

    const auto missing = server.handle(request(0x40U, 0x2222U, 0U));

    REQUIRE_EQ(missing->frame.data[0], 0x80U);
    REQUIRE_EQ(missing->frame.data[1], 0x22U);
    REQUIRE_EQ(missing->frame.data[2], 0x22U);
    REQUIRE_EQ(abort_code(missing->frame), 0x06020000U);
}

TEST_CASE(od_003_invalid_commands_and_nonexpedited_writes_abort) {
    CanopenObjectDictionary dictionary;
    CanopenSdoServer server(dictionary);

    REQUIRE_EQ(abort_code(server.handle(request(0x20U, 0x1017U, 0U))->frame),
               0x05040001U);
    REQUIRE_EQ(abort_code(server.handle(request(0x33U, 0x1017U, 0U))->frame),
               0x05040001U);
    REQUIRE_EQ(abort_code(server.handle(request(0xc0U, 0x1017U, 0U))->frame),
               0x05040001U);
}

TEST_CASE(od_003_wrong_identifier_or_size_is_not_an_sdo_request) {
    CanopenObjectDictionary dictionary;
    CanopenSdoServer server(dictionary);
    CanFrame wrong_identifier = request(0x40U, 0x1000U, 0U);
    wrong_identifier.identifier = 0x612U;
    CanFrame wrong_size = request(0x40U, 0x1000U, 0U);
    wrong_size.size = 7U;

    REQUIRE(!server.handle(wrong_identifier).has_value());
    REQUIRE(!server.handle(wrong_size).has_value());
}
