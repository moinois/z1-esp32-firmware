/** @file @brief Transport-independent HTTP response and routing policy. */
#pragma once

#include <cstdint>
#include <string_view>

namespace firmware::core {

/** Externally visible policy for one completed HTTP response. */
struct HttpResponsePolicy {
    /// Numeric HTTP status returned to the client.
    std::uint16_t status_code = 200U;
    /// MIME type for non-empty response content.
    std::string_view content_type = "text/html";
    /// Immutable fixed response body, if the policy supplies one.
    std::string_view body{};
    /// Selects streaming chunks instead of a known Content-Length.
    bool chunked_transfer = false;
    /// Forces connection closure when parser state cannot be reused safely.
    bool close_connection = false;
};

/** Parser failures with specified status, body, and connection behavior. */
enum class HttpParseError {
    unsupported_method,
    uri_too_long,
    unsupported_version,
    malformed_url,
    headers_too_long,
};

/** Builds a successful response using a fixed length unless streaming is selected. */
HttpResponsePolicy http_success_response(
    std::string_view content_type = "text/html", bool chunked_transfer = false);

/** Builds the exact response for a method unavailable on a resource. */
HttpResponsePolicy http_method_not_allowed_response();

/** Maps one parser failure to its exact status, HTML body, and close policy. */
HttpResponsePolicy http_response_for_parse_error(HttpParseError error);

/** Returns the case-sensitive URL path before the first query delimiter. */
std::string_view http_uri_path(std::string_view complete_uri);

/** Compares method tokens case-sensitively for handler selection. */
bool http_method_allowed(std::string_view request_method,
                         std::string_view handler_method);

}  // namespace firmware::core
