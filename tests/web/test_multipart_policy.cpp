// Verifies exact multipart boundary extraction limits and quoting rules.
#include "test.hpp"

#include "firmware/core/multipart_policy.hpp"

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
               std::optional<std::string_view>("quoted-boundary"));
}

TEST_CASE(webup_001_content_type_rejects_empty_missing_and_oversized_values) {
    REQUIRE(!parse_multipart_content_type("").has_value());
    REQUIRE(!parse_multipart_content_type("multipart/form-data").has_value());
    REQUIRE(!parse_multipart_content_type("boundary=").has_value());
    const std::string oversized(513U, 'x');
    REQUIRE(!parse_multipart_content_type(oversized).has_value());
}

TEST_CASE(webup_001_only_matching_outer_quotes_are_removed) {
    REQUIRE_EQ(parse_multipart_content_type("boundary=\"abc\""),
               std::optional<std::string_view>("abc"));
    REQUIRE_EQ(parse_multipart_content_type("boundary=\"abc"),
               std::optional<std::string_view>("\"abc"));
    REQUIRE_EQ(parse_multipart_content_type("boundary=abc\""),
               std::optional<std::string_view>("abc\""));
}
