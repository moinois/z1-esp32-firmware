/** @file @brief Block-local extraction of multipart update content. */
#pragma once

#include "core/protocol/bytes.hpp"

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

/** Applies the firmware protocol's intentionally block-local multipart rules. */
class MultipartPartExtractor {
public:
    /** Retains the exact boundary suffix, including quotes or emptiness. */
    explicit MultipartPartExtractor(std::string_view boundary);

    /** Processes one block independently and optionally marks end-of-input.
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

    /** Moves accepted bytes out so a target can write each receive block
     *  without retaining the complete uploaded image in RAM.
     */
    ByteVector take_content();

private:
    std::string boundary_;
    ByteVector content_;
    MultipartExtractStatus status_ = MultipartExtractStatus::reading_headers;
};

}  // namespace firmware::core
