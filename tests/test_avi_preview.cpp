// Verifies AVI preview acceptance, metadata retention, index parsing, and frame reads.
#include "test.hpp"

#include "firmware/core/avi_preview.hpp"

#include <cstdint>
#include <string_view>

using firmware::core::AviPreview;
using firmware::core::ByteVector;
using firmware::core::read_avi_frame;

namespace {

void append_u32(ByteVector& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16U));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24U));
}

void append_text(ByteVector& bytes, std::string_view text) {
    bytes.insert(bytes.end(), text.begin(), text.end());
}

void append_chunk(ByteVector& bytes, std::string_view id, const ByteVector& data) {
    append_text(bytes, id);
    append_u32(bytes, static_cast<std::uint32_t>(data.size()));
    bytes.insert(bytes.end(), data.begin(), data.end());
    if ((data.size() & 1U) != 0U) {
        bytes.push_back(0U);
    }
}

ByteVector make_avi() {
    ByteVector bytes;
    append_text(bytes, "RIFF");
    append_u32(bytes, 0U);
    append_text(bytes, "AVI ");

    ByteVector hdrl;
    ByteVector avih(56U, 0U);
    avih[0] = 0xA0U;
    avih[1] = 0x86U;
    avih[2] = 0x01U;
    avih[3] = 0U;
    append_chunk(hdrl, "avih", avih);
    ByteVector strf(20U, 0U);
    strf[0] = 20U;
    strf[4] = 0x80U;
    strf[5] = 0x02U;
    strf[8] = 0xE0U;
    strf[9] = 0x01U;
    append_chunk(hdrl, "strf", strf);
    ByteVector hdrl_list;
    append_text(hdrl_list, "hdrl");
    hdrl_list.insert(hdrl_list.end(), hdrl.begin(), hdrl.end());
    append_chunk(bytes, "LIST", hdrl_list);

    const std::uint32_t movi_data_offset = 12U + 8U +
        static_cast<std::uint32_t>(hdrl_list.size() + (hdrl_list.size() & 1U));
    ByteVector movi_data;
    append_text(movi_data, "movi");
    append_chunk(movi_data, "00dc", ByteVector({'A', 'B', 'C'}));
    append_chunk(bytes, "LIST", movi_data);

    ByteVector index;
    append_text(index, "00dc");
    append_u32(index, 0x10U);
    append_u32(index, 4U);
    append_u32(index, 3U);
    append_chunk(bytes, "idx1", index);
    const std::uint32_t riff_size = static_cast<std::uint32_t>(bytes.size() - 8U);
    bytes[4] = static_cast<std::uint8_t>(riff_size);
    bytes[5] = static_cast<std::uint8_t>(riff_size >> 8U);
    bytes[6] = static_cast<std::uint8_t>(riff_size >> 16U);
    bytes[7] = static_cast<std::uint8_t>(riff_size >> 24U);
    static_cast<void>(movi_data_offset);
    return bytes;
}

}  // namespace

TEST_CASE(avi_001_to_003_valid_file_retains_metadata_and_index) {
    const auto file = make_avi();
    const auto parsed = AviPreview::parse(file);
    REQUIRE(parsed.has_value());
    REQUIRE_EQ(parsed->frame_period_us, 100000U);
    REQUIRE_EQ(parsed->width, 640U);
    REQUIRE_EQ(parsed->height, 480U);
    REQUIRE_EQ(parsed->entries.size(), 1U);
    REQUIRE(parsed->movi_data_offset > 0U);
}

TEST_CASE(avi_004_frame_read_uses_movi_offset_and_validates_chunk) {
    const auto file = make_avi();
    const auto parsed = AviPreview::parse(file);
    REQUIRE(parsed.has_value());
    const auto frame = read_avi_frame(file, *parsed, 0U, 16U);
    REQUIRE_EQ(frame, std::optional<ByteVector>(ByteVector({'A', 'B', 'C'})));
    REQUIRE(!read_avi_frame(file, *parsed, 0U, 2U).has_value());
}

TEST_CASE(avi_001_short_or_wrong_header_is_rejected) {
    REQUIRE(!AviPreview::parse(ByteVector(31U, 0U)).has_value());
    ByteVector wrong(32U, 0U);
    append_text(wrong, "RIFF");
    REQUIRE(!AviPreview::parse(wrong).has_value());
}

TEST_CASE(avi_003_invalid_index_is_rejected_without_fallback) {
    auto file = make_avi();
    const std::size_t idx1 = file.size() - 24U;
    file[idx1 + 4U] = 3U;
    REQUIRE(!AviPreview::parse(file).has_value());
}
