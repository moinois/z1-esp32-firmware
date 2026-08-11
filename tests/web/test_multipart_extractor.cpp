// Verifies the specification's block-local multipart extraction semantics.
#include "test.hpp"

#include "core/web/multipart_extractor.hpp"

#include <string_view>

using firmware::core::ByteVector;
using firmware::core::MultipartExtractStatus;
using firmware::core::MultipartPartExtractor;

TEST_CASE(webup_002_blocks_are_processed_without_joining_adjacent_bytes) {
    MultipartPartExtractor extractor("AaB03x");
    REQUIRE(extractor.feed("prefix-AaB", false));
    REQUIRE(extractor.feed("03x-suffix", true));
    REQUIRE(extractor.status() == MultipartExtractStatus::complete);
    REQUIRE_EQ(extractor.content(),
               ByteVector({'p', 'r', 'e', 'f', 'i', 'x', '-', 'A', 'a', 'B',
                           '0', '3', 'x', '-', 's', 'u', 'f', 'f', 'i', 'x'}));
}

TEST_CASE(webup_002_detected_boundary_selects_or_discards_the_current_block) {
    MultipartPartExtractor extractor("boundary");
    REQUIRE(extractor.feed("--boundary\r\nheader\r\n\r\npayload", false));
    REQUIRE_EQ(extractor.content(), ByteVector({'p', 'a', 'y', 'l', 'o', 'a', 'd'}));
    REQUIRE(extractor.feed("discard-before-boundary", true));
    REQUIRE_EQ(extractor.content(), ByteVector({'p', 'a', 'y', 'l', 'o', 'a', 'd'}));
}

TEST_CASE(webup_002_embedded_nul_ends_marker_detection_but_not_delivery) {
    MultipartPartExtractor extractor("boundary");
    const ByteVector block{'a', '\0', 'b', 'o', 'u', 'n', 'd', 'a', 'r', 'y'};
    REQUIRE(extractor.feed(block, true));
    REQUIRE(extractor.status() == MultipartExtractStatus::complete);
    REQUIRE_EQ(extractor.content(), block);
}

TEST_CASE(webup_003_end_markers_have_no_special_treatment) {
    MultipartPartExtractor extractor("b");
    REQUIRE(extractor.feed("content\r\n--b--", true));
    REQUIRE(extractor.content().empty());
    REQUIRE(extractor.status() == MultipartExtractStatus::complete);
    REQUIRE(!extractor.feed("ignored", false));
}

TEST_CASE(webup_003_empty_boundary_discards_blocks_without_header_terminators) {
    MultipartPartExtractor extractor("");
    REQUIRE(extractor.feed("discarded", false));
    REQUIRE(extractor.feed("\r\n\r\naccepted", true));
    REQUIRE_EQ(extractor.content(),
               ByteVector({'a', 'c', 'c', 'e', 'p', 't', 'e', 'd'}));
}

TEST_CASE(webup_003_transport_end_completes_even_without_multipart_headers) {
    MultipartPartExtractor extractor("missing");
    REQUIRE(extractor.feed("raw bytes", true));
    REQUIRE(extractor.status() == MultipartExtractStatus::complete);
    REQUIRE_EQ(extractor.content(),
               ByteVector({'r', 'a', 'w', ' ', 'b', 'y', 't', 'e', 's'}));
}
