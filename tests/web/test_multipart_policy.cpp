// Verifies exact multipart boundary extraction limits and suffix preservation.
#include "test.hpp"

#include "core/web/multipart_policy.hpp"

#include <string_view>

using firmware::core::parse_multipart_content_type;

TEST_CASE(webup_001_content_type_requires_boundary_and_uses_the_full_suffix) {
    REQUIRE_EQ(parse_multipart_content_type("multipart/form-data; boundary=abc"),
               std::optional<std::string_view>("abc"));
    REQUIRE_EQ(parse_multipart_content_type(
                   "multipart/form-data; boundary=abc; charset=binary"),
               std::optional<std::string_view>("abc; charset=binary"));
    REQUIRE_EQ(parse_multipart_content_type(
                   "multipart/form-data; boundary=\"quoted-boundary\""),
               std::optional<std::string_view>("\"quoted-boundary\""));
}

TEST_CASE(webup_001_content_type_accepts_empty_boundary_but_rejects_missing_marker) {
    REQUIRE(!parse_multipart_content_type("").has_value());
    REQUIRE(!parse_multipart_content_type("multipart/form-data").has_value());
    REQUIRE_EQ(parse_multipart_content_type("boundary="),
               std::optional<std::string_view>(""));
    const std::string oversized(513U, 'x');
    REQUIRE(!parse_multipart_content_type(oversized).has_value());
}

TEST_CASE(webup_001_quotes_are_part_of_the_boundary_suffix) {
    REQUIRE_EQ(parse_multipart_content_type("boundary=\"abc\""),
               std::optional<std::string_view>("\"abc\""));
    REQUIRE_EQ(parse_multipart_content_type("boundary=\"abc"),
               std::optional<std::string_view>("\"abc"));
    REQUIRE_EQ(parse_multipart_content_type("boundary=abc\""),
               std::optional<std::string_view>("abc\""));
}
