// Tests the checksums shared by transport, path identity, BLUFI, and OTA.
#include "test.hpp"
#include "firmware/core/crc.hpp"

#include <string_view>

using firmware::core::BytesView;

TEST_CASE(frm_004_crc16_ccitt_uses_zero_initial_value) {
    const std::string_view value = "123456789";
    REQUIRE_EQ(firmware::core::crc16_ccitt(BytesView(value)), 0x31C3U);
}

TEST_CASE(blesec_005_blufi_crc_uses_ffff_initial_and_final_xor) {
    const std::string_view value = "123456789";
    REQUIRE_EQ(firmware::core::crc16_blufi(BytesView(value)), 0xD64EU);
}

TEST_CASE(upd_012_crc32_matches_iso_hdlc_reference_vector) {
    const std::string_view value = "123456789";
    REQUIRE_EQ(firmware::core::crc32_iso_hdlc(BytesView(value)), 0xCBF43926UL);
}
