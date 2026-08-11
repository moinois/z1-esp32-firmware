// Verifies static-path, MIME, and firmware-identity rules without a web server.
#include "test.hpp"

#include "core/web/web_static.hpp"

#include <string>
#include <string_view>

using firmware::core::firmware_identity_json;
using firmware::core::resolve_static_path;
using firmware::core::static_mime_type;

TEST_CASE(web_010_static_paths_append_the_complete_uri_verbatim) {
    REQUIRE_EQ(resolve_static_path("/assets/app.js?revision=7"),
               std::optional<std::string>("/spiffs/assets/app.js?revision=7"));
    REQUIRE_EQ(resolve_static_path("/"),
               std::optional<std::string>("/spiffs/index.html"));
    REQUIRE_EQ(resolve_static_path("/nested/"),
               std::optional<std::string>("/spiffs/nested/index.html"));
    REQUIRE_EQ(resolve_static_path("/../secret"),
               std::optional<std::string>("/spiffs/../secret"));
    REQUIRE_EQ(resolve_static_path("/%2e%2e/secret"),
               std::optional<std::string>("/spiffs/%2e%2e/secret"));
}

TEST_CASE(web_010_static_paths_truncate_to_the_255_byte_buffer_extent) {
    const std::string longest_uri(
        firmware::core::web::static_path_capacity -
            firmware::core::web::volume_prefix.size() - 1U,
        'a');
    REQUIRE(resolve_static_path(longest_uri).has_value());
    const auto truncated = resolve_static_path(longest_uri + "abcdef");
    REQUIRE(truncated.has_value());
    REQUIRE_EQ(truncated->size(), 255U);
    REQUIRE_EQ(*truncated, std::string(firmware::core::web::volume_prefix) +
                               longest_uri);

    const std::string nearly_full_uri(
        firmware::core::web::static_path_capacity -
            firmware::core::web::volume_prefix.size() - 4U,
        'd');
    REQUIRE_EQ(resolve_static_path(nearly_full_uri + "/"),
               std::optional<std::string>(
                   std::string(firmware::core::web::volume_prefix) +
                   nearly_full_uri + "/in"));
}

TEST_CASE(web_012_mime_type_uses_the_specified_substring_precedence) {
    REQUIRE_EQ(static_mime_type("/style.css.html.js"),
               std::string_view("text/html"));
    REQUIRE_EQ(static_mime_type("/styles/maincss"), std::string_view("text/plain"));
    REQUIRE_EQ(static_mime_type("/styles/main.css?x"), std::string_view("text/css"));
    REQUIRE_EQ(static_mime_type("/script.js?v=1"),
               std::string_view("application/javascript"));
    REQUIRE_EQ(static_mime_type("/IMAGE.CSS"), std::string_view("text/plain"));
    REQUIRE_EQ(static_mime_type("/image.jpeg"), std::string_view("text/plain"));
}

TEST_CASE(web_011_and_013_fixed_web_payloads_are_exact) {
    REQUIRE_EQ(firmware::core::web::missing_static_file_body,
               std::string_view("Nothing matches the given URI"));
    REQUIRE_EQ(firmware::core::web::static_file_chunk_size, 256U);
    REQUIRE_EQ(firmware_identity_json(),
               std::string_view(
                   "{\"version\":\"0.1.13\",\"build_date\":\"2026.08.05\","
                   "\"idf_ver\":\"v5.4.1\"}"));
}
