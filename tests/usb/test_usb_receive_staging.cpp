// Verifies USB receive capacity and all-or-nothing block staging.
#include "test.hpp"

#include "application/usb/usb_receive_staging.hpp"

using firmware::application::UsbReceiveStaging;
using firmware::core::ByteVector;

TEST_CASE(usb_006_staging_accepts_blocks_until_the_32768_byte_limit) {
    UsbReceiveStaging staging;
    REQUIRE(staging.stage(ByteVector(16000U, 0x11U)));
    REQUIRE(staging.stage(ByteVector(16768U, 0x22U)));
    REQUIRE_EQ(staging.size(), UsbReceiveStaging::capacity);
    const auto bytes = staging.take();
    REQUIRE_EQ(bytes.size(), UsbReceiveStaging::capacity);
    REQUIRE_EQ(bytes.front(), 0x11U);
    REQUIRE_EQ(bytes.back(), 0x22U);
}

TEST_CASE(usb_006_failed_block_discards_the_entire_partial_session) {
    UsbReceiveStaging staging;
    REQUIRE(staging.stage(ByteVector(32000U, 0x33U)));
    REQUIRE(!staging.stage(ByteVector(1000U, 0x44U)));
    REQUIRE_EQ(staging.size(), 0U);
    REQUIRE(staging.take().empty());
}

TEST_CASE(usb_006_disconnect_clear_discards_buffered_bytes) {
    UsbReceiveStaging staging;
    REQUIRE(staging.stage(ByteVector(64U, 0x55U)));
    staging.clear();
    REQUIRE_EQ(staging.size(), 0U);
}

