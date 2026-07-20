// Implements streaming first-part multipart extraction and header limits.
#include "firmware/core/multipart_extractor.hpp"

#include <algorithm>

namespace firmware::core {
namespace {

constexpr std::size_t maximum_part_header_bytes = 4096U;
constexpr std::string_view header_terminator = "\r\n\r\n";

// Builds the exact marker that terminates the first multipart part.
std::string content_boundary_marker(std::string_view boundary) {
    std::string marker("\r\n--");
    marker.append(boundary);
    return marker;
}

}  // namespace

MultipartPartExtractor::MultipartPartExtractor(std::string_view boundary)
    : boundary_(boundary) {}

bool MultipartPartExtractor::feed(BytesView block, bool transport_finished) {
    if (status_ == MultipartExtractStatus::complete ||
        status_ == MultipartExtractStatus::failed) {
        return false;
    }
    pending_.append(reinterpret_cast<const char*>(block.data()), block.size());
    if (status_ == MultipartExtractStatus::reading_headers &&
        !process_headers(transport_finished)) {
        return false;
    }
    if (status_ == MultipartExtractStatus::reading_content) {
        return process_content(transport_finished);
    }
    return status_ != MultipartExtractStatus::failed;
}

MultipartExtractStatus MultipartPartExtractor::status() const {
    return status_;
}

const ByteVector& MultipartPartExtractor::content() const {
    return content_;
}

bool MultipartPartExtractor::process_headers(bool transport_finished) {
    const std::size_t terminator = pending_.find(header_terminator);
    if (terminator == std::string::npos) {
        if (pending_.size() > maximum_part_header_bytes || transport_finished) {
            status_ = MultipartExtractStatus::failed;
            return false;
        }
        return true;
    }
    pending_.erase(0U, terminator + header_terminator.size());
    status_ = MultipartExtractStatus::reading_content;
    return true;
}

bool MultipartPartExtractor::process_content(bool transport_finished) {
    const std::string marker = content_boundary_marker(boundary_);
    const std::size_t boundary_start = pending_.find(marker);
    if (boundary_start != std::string::npos) {
        content_.insert(content_.end(), pending_.begin(),
                        pending_.begin() + static_cast<std::ptrdiff_t>(boundary_start));
        status_ = MultipartExtractStatus::complete;
        pending_.clear();
        return true;
    }
    if (transport_finished) {
        content_.insert(content_.end(), pending_.begin(), pending_.end());
        pending_.clear();
        status_ = MultipartExtractStatus::complete;
    }
    return true;
}

}  // namespace firmware::core
