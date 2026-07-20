// Verifies the common wire parsing and inbox policy for controller transfers.
#include "test.hpp"

#include "firmware/application/controller_transfer.hpp"

using firmware::application::ControllerTransferInbox;
using firmware::application::TransferOperation;
using firmware::core::ByteVector;
using firmware::core::Frame;

TEST_CASE(lpc_010_low_nibbles_select_the_common_transfer_operations) {
    REQUIRE_EQ(firmware::application::transfer_operation(0xC1U), TransferOperation::start);
    REQUIRE_EQ(firmware::application::transfer_operation(0xD2U), TransferOperation::geometry);
    REQUIRE_EQ(firmware::application::transfer_operation(0xE3U), TransferOperation::data);
    REQUIRE_EQ(firmware::application::transfer_operation(0xC4U), TransferOperation::complete);
    REQUIRE_EQ(firmware::application::transfer_operation(0xD5U), TransferOperation::cancel);
    REQUIRE_EQ(firmware::application::transfer_operation(0xEFU), TransferOperation::unknown);
}

TEST_CASE(lpc_011_each_family_inbox_is_bounded_and_fifo) {
    ControllerTransferInbox inbox(0xC0U);

    for (std::uint8_t value = 0U; value < 32U; ++value) {
        REQUIRE(inbox.enqueue({0xC3U, {value}}));
    }
    REQUIRE(!inbox.enqueue({0xC3U, {32U}}));
    REQUIRE_EQ(inbox.pending(), 32U);
    REQUIRE_EQ(inbox.take_ready(0U)->payload, ByteVector({0U}));
    REQUIRE_EQ(inbox.take_ready(10U)->payload, ByteVector({1U}));
}

TEST_CASE(lpc_011_family_inbox_rejects_wrong_family_and_items_over_544_bytes) {
    ControllerTransferInbox inbox(0xD0U);

    REQUIRE(!inbox.enqueue({0xC1U, {}}));
    REQUIRE(inbox.enqueue({0xD3U, ByteVector(535U, 0xAAU)}));
    REQUIRE(!inbox.enqueue({0xD3U, ByteVector(536U, 0xAAU)}));
}

TEST_CASE(lpc_011_family_inbox_processes_no_more_than_once_per_ten_milliseconds) {
    ControllerTransferInbox inbox(0xE0U);
    REQUIRE(inbox.enqueue({0xE1U, {}}));
    REQUIRE(inbox.enqueue({0xE2U, {}}));

    REQUIRE(inbox.take_ready(75U).has_value());
    REQUIRE(!inbox.take_ready(84U).has_value());
    REQUIRE(inbox.take_ready(85U).has_value());
}

TEST_CASE(lpc_012_geometry_is_unsigned_big_endian_and_requires_six_bytes) {
    const auto geometry = firmware::application::parse_transfer_geometry(
        ByteVector({0x12U, 0x34U, 0x56U, 0x78U, 0x01U, 0xF4U, 0xAAU}));

    REQUIRE(geometry.has_value());
    REQUIRE_EQ(geometry->frame_count, 0x12345678U);
    REQUIRE_EQ(geometry->frame_data_size, 500U);
    REQUIRE(!firmware::application::parse_transfer_geometry(ByteVector(5U, 0U)).has_value());
}

TEST_CASE(lpc_014_data_index_is_big_endian_and_preserves_its_wire_bytes) {
    const ByteVector payload{0x89U, 0xABU, 0xCDU, 0xEFU, 0x55U};
    const auto request = firmware::application::parse_transfer_data_request(payload);

    REQUIRE(request.has_value());
    REQUIRE_EQ(request->index, 0x89ABCDEFU);
    REQUIRE_EQ(request->wire_index, ByteVector({0x89U, 0xABU, 0xCDU, 0xEFU}));
    REQUIRE(!firmware::application::parse_transfer_data_request(ByteVector(3U, 0U)).has_value());
}

TEST_CASE(lpc_010_reply_uses_the_same_family_and_requested_low_nibble) {
    const Frame reply = firmware::application::make_transfer_reply(0xD0U, 3U, {1U, 2U});

    REQUIRE_EQ(reply.type, 0xD3U);
    REQUIRE_EQ(reply.payload, ByteVector({1U, 2U}));
}
