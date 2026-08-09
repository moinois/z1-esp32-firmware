// Verifies HTTP response and request-selection policy independently of ESP-IDF.
#include "test.hpp"

#include "core/web/http_policy.hpp"

#include <string_view>

using firmware::core::HttpParseError;
using firmware::core::http_response_for_parse_error;
using firmware::core::http_success_response;
using firmware::core::http_uri_path;
using firmware::core::http_method_allowed;
using firmware::core::http_method_not_allowed_response;

TEST_CASE(web_007_success_responses_use_content_length_and_html_by_default) {
    const auto response = http_success_response();
    REQUIRE_EQ(response.status_code, 200U);
    REQUIRE_EQ(response.content_type, std::string_view("text/html"));
    REQUIRE(!response.chunked_transfer);
    REQUIRE(!response.close_connection);

    const auto json = http_success_response("application/json");
    REQUIRE_EQ(json.content_type, std::string_view("application/json"));
}

TEST_CASE(web_007_static_and_method_failures_have_exact_response_policy) {
    const auto static_response = http_success_response("text/plain", true);
    REQUIRE(static_response.chunked_transfer);
    REQUIRE(!static_response.close_connection);

    const auto method_response = http_method_not_allowed_response();
    REQUIRE_EQ(method_response.status_code, 405U);
    REQUIRE_EQ(method_response.content_type, std::string_view("text/html"));
    REQUIRE_EQ(method_response.body,
               std::string_view("Specified method is invalid for this resource"));
    REQUIRE(method_response.close_connection);
}

TEST_CASE(web_009_parser_errors_map_to_exact_status_body_and_close) {
    const auto unsupported_method =
        http_response_for_parse_error(HttpParseError::unsupported_method);
    REQUIRE_EQ(unsupported_method.status_code, 501U);
    REQUIRE_EQ(unsupported_method.body,
               std::string_view("Server does not support this method"));

    const auto long_uri = http_response_for_parse_error(HttpParseError::uri_too_long);
    REQUIRE_EQ(long_uri.status_code, 414U);
    REQUIRE_EQ(long_uri.body, std::string_view("URI is too long"));

    const auto version = http_response_for_parse_error(HttpParseError::unsupported_version);
    REQUIRE_EQ(version.status_code, 505U);
    REQUIRE_EQ(version.body,
               std::string_view("HTTP version not supported by server"));

    const auto syntax = http_response_for_parse_error(HttpParseError::malformed_url);
    REQUIRE_EQ(syntax.status_code, 400U);
    REQUIRE_EQ(syntax.body, std::string_view("Bad request syntax"));

    const auto headers = http_response_for_parse_error(HttpParseError::headers_too_long);
    REQUIRE_EQ(headers.status_code, 431U);
    REQUIRE_EQ(headers.body, std::string_view("Header fields are too long"));
    REQUIRE(headers.close_connection);
    REQUIRE_EQ(headers.content_type, std::string_view("text/html"));
}

TEST_CASE(web_008_handler_matching_excludes_query_but_preserves_complete_uri) {
    REQUIRE_EQ(http_uri_path("/api/update?mode=fast"), std::string_view("/api/update"));
    REQUIRE_EQ(http_uri_path("/api/update"), std::string_view("/api/update"));
    REQUIRE_EQ(http_uri_path("?only=query"), std::string_view());
    REQUIRE(http_method_allowed("GET", "GET"));
    REQUIRE(!http_method_allowed("get", "GET"));
    REQUIRE(!http_method_allowed("POST", "GET"));
}
