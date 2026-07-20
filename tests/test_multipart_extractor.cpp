// Verifies first-part multipart extraction across arbitrary transport blocks.
#include "test.hpp"

#include "firmware/core/multipart_extractor.hpp"

#include <string_view>

using firmware::core::ByteVector;
using firmware::core::MultipartExtractStatus;
using firmware::core::MultipartPartExtractor;

TEST_CASE(webup_002_headers_and_boundary_markers_may_span_blocks) {
    MultipartPartExtractor extractor("AaB03x");
    REQUIRE(extractor.feed("Content-Disposition: form-data\r\n", false));
    REQUIRE(extractor.status() == MultipartExtractStatus::reading_headers);
    REQUIRE(extractor.feed("\r\nhello world\r\n--AaB", false));
    REQUIRE(extractor.status() == MultipartExtractStatus::reading_content);
    REQUIRE(extractor.feed("03x--\r\nignored", false));
    REQUIRE(extractor.status() == MultipartExtractStatus::complete);
    REQUIRE_EQ(extractor.content(), ByteVector({'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'}));
}

TEST_CASE(webup_002_header_limit_is_checked_after_searching_for_terminator) {
    MultipartPartExtractor extractor("boundary");
    const std::string block(4096U, 'h');
    REQUIRE(extractor.feed(block, false));
    REQUIRE(extractor.status() == MultipartExtractStatus::reading_headers);
    REQUIRE(!extractor.feed("x", false));
    REQUIRE(extractor.status() == MultipartExtractStatus::failed);

    MultipartPartExtractor terminator_in_large_block("boundary");
    const std::string large_header(4096U, 'h');
    REQUIRE(terminator_in_large_block.feed(large_header + "\r\n\r\nbody", false));
    REQUIRE(terminator_in_large_block.status() ==
            MultipartExtractStatus::reading_content);
}

TEST_CASE(webup_003_end_after_headers_accepts_remaining_content_without_boundary) {
    MultipartPartExtractor extractor("boundary");
    REQUIRE(extractor.feed("part headers\r\n\r\nremaining", true));
    REQUIRE(extractor.status() == MultipartExtractStatus::complete);
    REQUIRE_EQ(extractor.content(), ByteVector({'r', 'e', 'm', 'a', 'i', 'n', 'i', 'n', 'g'}));

    MultipartPartExtractor incomplete("boundary");
    REQUIRE(!incomplete.feed("headers without terminator", true));
    REQUIRE(incomplete.status() == MultipartExtractStatus::failed);
}

TEST_CASE(webup_002_first_boundary_marker_ends_content) {
    MultipartPartExtractor extractor("b");
    REQUIRE(extractor.feed("h\r\n\r\nfirst\r\n--b\r\nsecond", false));
    REQUIRE_EQ(extractor.content(), ByteVector({'f', 'i', 'r', 's', 't'}));
    REQUIRE(extractor.status() == MultipartExtractStatus::complete);
    REQUIRE(!extractor.feed("ignored", false));
}
