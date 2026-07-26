// Implements HTTP response mapping and case-sensitive URI-handler selection.
#include "firmware/core/http_policy.hpp"

namespace firmware::core {
namespace {

constexpr std::string_view html_content_type = "text/html";
constexpr std::string_view method_not_allowed_body =
    "Specified method is invalid for this resource";

// Returns the fixed status and body pair associated with a parser failure.
HttpResponsePolicy make_parser_response(std::uint16_t status,
                                        std::string_view body) {
    return {status, html_content_type, body, false, true};
}

}  // namespace

HttpResponsePolicy http_success_response(std::string_view content_type,
                                         bool chunked_transfer) {
    return {200U, content_type, {}, chunked_transfer, false};
}

HttpResponsePolicy http_method_not_allowed_response() {
    return {405U, html_content_type, method_not_allowed_body, false, true};
}

HttpResponsePolicy http_response_for_parse_error(HttpParseError error) {
    switch (error) {
        case HttpParseError::unsupported_method:
            return make_parser_response(501U, "Server does not support this method");
        case HttpParseError::uri_too_long:
            return make_parser_response(414U, "URI is too long");
        case HttpParseError::unsupported_version:
            return make_parser_response(505U,
                                         "HTTP version not supported by server");
        case HttpParseError::malformed_url:
            return make_parser_response(400U, "Bad request syntax");
        case HttpParseError::headers_too_long:
            return make_parser_response(431U, "Header fields are too long");
    }
    return make_parser_response(400U, "Bad request syntax");
}

std::string_view http_uri_path(std::string_view complete_uri) {
    const std::size_t query_start = complete_uri.find('?');
    return complete_uri.substr(0U, query_start);
}

bool http_method_allowed(std::string_view request_method,
                         std::string_view handler_method) {
    return request_method == handler_method;
}

}  // namespace firmware::core
