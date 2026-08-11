// Tests aggregate firmware header decoding and validation before any flash operation.
#include "test.hpp"
#include "core/update/update_package.hpp"

using firmware::core::ByteVector;

namespace {

void put_le32(ByteVector& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (8U * index));
    }
}

ByteVector valid_package() {
    ByteVector bytes(35, 0);
    put_le32(bytes, 0, 0x4D5173EEUL);
    bytes[4] = 1;
    bytes[5] = 32;
    bytes[6] = 1;
    put_le32(bytes, 8, 3);
    bytes[32] = 0xE9;
    bytes[33] = 1;
    bytes[34] = 2;
    put_le32(bytes, 24, firmware::core::crc32_iso_hdlc({bytes.data(), 24}));
    const std::uint32_t crc = firmware::core::aggregate_file_crc(bytes);
    put_le32(bytes, 28, crc);
    return bytes;
}

}  // namespace

TEST_CASE(upd_010_header_is_little_endian_and_exactly_32_bytes) {
    const auto result = firmware::core::parse_update_package(valid_package());
    REQUIRE(result.valid());
    REQUIRE_EQ(result.header->mainboard_size, 3UL);
    REQUIRE_EQ(result.header->controller_size, 0UL);
}

TEST_CASE(upd_011_flags_must_match_nonzero_image_sizes) {
    auto bytes = valid_package();
    bytes[6] = 0;
    put_le32(bytes, 24, firmware::core::crc32_iso_hdlc({bytes.data(), 24}));
    put_le32(bytes, 28, firmware::core::aggregate_file_crc(bytes));
    REQUIRE_EQ(firmware::core::parse_update_package(bytes).error, firmware::core::UpdateError::flags);
}

TEST_CASE(upd_012_file_crc_skips_its_stored_field) {
    auto bytes = valid_package();
    const auto crc = firmware::core::aggregate_file_crc(bytes);
    bytes[28] ^= 0xFF;
    REQUIRE_EQ(firmware::core::aggregate_file_crc(bytes), crc);
    REQUIRE_EQ(firmware::core::parse_update_package(bytes).error, firmware::core::UpdateError::file_crc);
}

TEST_CASE(upd_013_declared_images_must_fit_but_trailing_bytes_are_allowed) {
    auto bytes = valid_package();
    bytes.push_back(0xAA);
    put_le32(bytes, 28, firmware::core::aggregate_file_crc(bytes));
    REQUIRE(firmware::core::parse_update_package(bytes).valid());
    put_le32(bytes, 8, 1000);
    put_le32(bytes, 24, firmware::core::crc32_iso_hdlc({bytes.data(), 24}));
    put_le32(bytes, 28, firmware::core::aggregate_file_crc(bytes));
    REQUIRE_EQ(firmware::core::parse_update_package(bytes).error, firmware::core::UpdateError::size);
}

TEST_CASE(upd_013_required_size_uses_wrapping_32_bit_arithmetic) {
    ByteVector bytes(32U, 0U);
    put_le32(bytes, 0U, 0x4D5173EEUL);
    bytes[4] = 1U;
    bytes[5] = 32U;
    bytes[6] = 2U;
    put_le32(bytes, 12U, 0xffffffe0U);
    put_le32(bytes, 24U, firmware::core::crc32_iso_hdlc({bytes.data(), 24U}));
    put_le32(bytes, 28U, firmware::core::aggregate_file_crc(bytes));
    REQUIRE(firmware::core::parse_update_package(bytes).valid());
}

TEST_CASE(upd_010_rejects_short_magic_version_and_header_length) {
    REQUIRE_EQ(firmware::core::aggregate_file_crc(ByteVector(31U, 0U)), 0U);
    REQUIRE_EQ(firmware::core::parse_update_package(ByteVector(31U, 0U)).error,
               firmware::core::UpdateError::short_file);

    auto bytes = valid_package();
    bytes[0] ^= 0xFFU;
    REQUIRE_EQ(firmware::core::parse_update_package(bytes).error,
               firmware::core::UpdateError::magic);

    for (const std::uint8_t version : {0U, 3U}) {
        bytes = valid_package();
        bytes[4] = version;
        REQUIRE_EQ(firmware::core::parse_update_package(bytes).error,
                   firmware::core::UpdateError::version);
    }

    bytes = valid_package();
    bytes[5] = 31U;
    REQUIRE_EQ(firmware::core::parse_update_package(bytes).error,
               firmware::core::UpdateError::header_length);
}

TEST_CASE(upd_012_rejects_header_crc_and_non_esp_mainboard_image) {
    auto bytes = valid_package();
    bytes[24] ^= 0xFFU;
    REQUIRE_EQ(firmware::core::parse_update_package(bytes).error,
               firmware::core::UpdateError::header_crc);

    bytes = valid_package();
    bytes[32] = 0x00U;
    put_le32(bytes, 28, firmware::core::aggregate_file_crc(bytes));
    REQUIRE_EQ(firmware::core::parse_update_package(bytes).error,
               firmware::core::UpdateError::image);
}
