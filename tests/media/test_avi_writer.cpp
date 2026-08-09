// Verifies recorded AVI header fields, padding, and indexed frame layout.
#include "test.hpp"
#include "core/media/avi_writer.hpp"

TEST_CASE(avi_010_to_013_writer_emits_header_frames_and_index) {
    firmware::core::AviWriter writer(640U, 480U);
    const std::string jpeg(2048U, 'j');
    REQUIRE(writer.append_frame(firmware::core::BytesView(jpeg)));
    const auto result = writer.finalize();
    REQUIRE(result.has_value());
    REQUIRE_EQ(result->size(), 224U + 4U + 4U + 2048U + 2048U + 8U + 16U);
    REQUIRE_EQ((*result)[0], static_cast<std::uint8_t>('R'));
    REQUIRE_EQ((*result)[32], static_cast<std::uint8_t>(0xa0U));
    REQUIRE_EQ((*result)[48], static_cast<std::uint8_t>(1U));
    REQUIRE_EQ((*result)[result->size() - 24U], static_cast<std::uint8_t>('i'));
    REQUIRE_EQ((*result)[result->size() - 16U], static_cast<std::uint8_t>('0'));
    REQUIRE_EQ((*result)[result->size() - 12U], static_cast<std::uint8_t>(0x10U));
    REQUIRE(!writer.finalize().has_value());
}
