// Declares a bounded-header, streaming extractor for the first multipart part.
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace firmware::core {

// Describes the lifecycle of one multipart first-part extraction.
enum class MultipartExtractStatus {
    reading_headers,
    reading_content,
    complete,
    failed,
};

// Extracts part content while retaining data across arbitrary transport blocks.
class MultipartPartExtractor {
public:
    // Retains the boundary used to identify the first part's content terminator.
    explicit MultipartPartExtractor(std::string_view boundary);

    // Feeds one block and optionally marks transport end-of-input.
    bool feed(BytesView block, bool transport_finished);

    // Offers a text-view convenience overload for host and adapter callers.
    bool feed(std::string_view block, bool transport_finished) {
        return feed(BytesView(block), transport_finished);
    }

    // Reports the current extraction lifecycle state.
    MultipartExtractStatus status() const;

    // Returns all bytes accepted as the first part's content.
    const ByteVector& content() const;

private:
    // Searches buffered headers and transitions to content mode when complete.
    bool process_headers(bool transport_finished);

    // Searches for a split-safe boundary marker and finalizes content if found.
    bool process_content(bool transport_finished);

    std::string boundary_;
    std::string pending_;
    ByteVector content_;
    MultipartExtractStatus status_ = MultipartExtractStatus::reading_headers;
};

}  // namespace firmware::core
