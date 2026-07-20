// Declares transport-independent HTTP response and request-selection policy.
#pragma once

#include <cstdint>
#include <string_view>

namespace firmware::core {

// Describes the externally visible parts of one completed HTTP response.
struct HttpResponsePolicy {
    std::uint16_t status_code = 200U;
    std::string_view content_type = "text/html";
    std::string_view body{};
    bool chunked_transfer = false;
    bool close_connection = false;
};

// Names parser failures that have specified HTTP status and body text.
enum class HttpParseError {
    unsupported_method,
    uri_too_long,
    unsupported_version,
    malformed_url,
    headers_too_long,
};

// Builds a successful response, using content length unless streaming is selected.
HttpResponsePolicy http_success_response(
    std::string_view content_type = "text/html", bool chunked_transfer = false);

// Builds the exact response for a method that is not available on a resource.
HttpResponsePolicy http_method_not_allowed_response();

// Maps one parser failure to its exact status, HTML body, and close policy.
HttpResponsePolicy http_response_for_parse_error(HttpParseError error);

// Returns the case-sensitive URL path before the first query delimiter.
std::string_view http_uri_path(std::string_view complete_uri);

// Compares method tokens case-sensitively for URI-handler selection.
bool http_method_allowed(std::string_view request_method,
                         std::string_view handler_method);

}  // namespace firmware::core
