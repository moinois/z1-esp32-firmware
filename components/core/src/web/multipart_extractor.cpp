/** @file @brief Implements the protocol's block-local multipart extraction. */
#include "core/web/multipart_extractor.hpp"

#include <algorithm>

namespace firmware::core {
namespace {

constexpr std::string_view header_terminator = "\r\n\r\n";

}  // namespace

MultipartPartExtractor::MultipartPartExtractor(std::string_view boundary)
    : boundary_(boundary) {}

bool MultipartPartExtractor::feed(BytesView block, bool transport_finished) {
    return feed(block, block.size(), transport_finished);
}

bool MultipartPartExtractor::feed(BytesView accessible, std::size_t current_size,
                                  bool transport_finished) {
    if (status_ == MultipartExtractStatus::complete ||
        status_ == MultipartExtractStatus::failed ||
        current_size > accessible.size()) {
        return false;
    }
    const auto nul = std::find(accessible.begin(), accessible.end(), std::uint8_t{0});
    const std::string_view detection(
        reinterpret_cast<const char*>(accessible.data()),
        static_cast<std::size_t>(nul - accessible.begin()));
    const std::size_t boundary = detection.find(boundary_);
    last_boundary_detected_ = boundary != std::string_view::npos;
    if (boundary == std::string_view::npos) {
        content_.insert(content_.end(), accessible.begin(),
                        accessible.begin() + static_cast<std::ptrdiff_t>(current_size));
        status_ = MultipartExtractStatus::reading_content;
    } else {
        const std::size_t terminator = detection.find(header_terminator, boundary);
        if (terminator != std::string_view::npos) {
            const std::size_t content_start = terminator + header_terminator.size();
            if (content_start <= current_size) {
                content_.insert(content_.end(), accessible.begin() +
                                    static_cast<std::ptrdiff_t>(content_start),
                                accessible.begin() +
                                    static_cast<std::ptrdiff_t>(current_size));
            }
            status_ = MultipartExtractStatus::reading_content;
        }
    }
    if (transport_finished) status_ = MultipartExtractStatus::complete;
    return true;
}

MultipartExtractStatus MultipartPartExtractor::status() const {
    return status_;
}

const ByteVector& MultipartPartExtractor::content() const {
    return content_;
}

ByteVector MultipartPartExtractor::take_content() {
    ByteVector accepted;
    accepted.swap(content_);
    return accepted;
}

bool MultipartPartExtractor::last_boundary_detected() const {
    return last_boundary_detected_;
}

}  // namespace firmware::core
