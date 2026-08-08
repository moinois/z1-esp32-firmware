/** @file @brief Bounded streaming extraction of the first multipart part. */
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace firmware::core {

/** Lifecycle of a first-part multipart extraction. */
enum class MultipartExtractStatus {
    reading_headers,
    reading_content,
    complete,
    failed,
};

/** Extracts first-part content across arbitrary transport block boundaries. */
class MultipartPartExtractor {
public:
    /** Retains the validated boundary used to find the content terminator. */
    explicit MultipartPartExtractor(std::string_view boundary);

    /** Feeds one block and optionally marks transport end-of-input.
     *  @return False once parsing has failed; true for progress or completion.
     */
    bool feed(BytesView block, bool transport_finished);

    /// Text-view convenience overload for HTTP adapter callers.
    bool feed(std::string_view block, bool transport_finished) {
        return feed(BytesView(block), transport_finished);
    }

    /// Reports the current extraction lifecycle state.
    MultipartExtractStatus status() const;

    /// Returns all accepted content; valid until this extractor is destroyed.
    const ByteVector& content() const;

private:
    /// Searches bounded headers and transitions to content mode when complete.
    bool process_headers(bool transport_finished);

    /// Searches for a split-safe terminator and finalizes content when found.
    bool process_content(bool transport_finished);

    std::string boundary_;
    std::string pending_;
    ByteVector content_;
    MultipartExtractStatus status_ = MultipartExtractStatus::reading_headers;
};

}  // namespace firmware::core
