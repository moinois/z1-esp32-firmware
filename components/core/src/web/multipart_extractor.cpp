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
    if (status_ == MultipartExtractStatus::complete ||
        status_ == MultipartExtractStatus::failed) {
        return false;
    }
    const auto nul = std::find(block.begin(), block.end(), std::uint8_t{0});
    const std::string_view detection(
        reinterpret_cast<const char*>(block.data()),
        static_cast<std::size_t>(nul - block.begin()));
    const std::size_t boundary = detection.find(boundary_);
    if (boundary == std::string_view::npos) {
        content_.insert(content_.end(), block.begin(), block.end());
        status_ = MultipartExtractStatus::reading_content;
    } else {
        const std::size_t terminator = detection.find(header_terminator, boundary);
        if (terminator != std::string_view::npos) {
            const std::size_t content_start = terminator + header_terminator.size();
            content_.insert(content_.end(), block.begin() +
                                static_cast<std::ptrdiff_t>(content_start),
                            block.end());
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

}  // namespace firmware::core
