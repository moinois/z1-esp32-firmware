/** @file @brief Bounded in-memory MJPEG AVI assembly for recorded segments. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace firmware::core {

/** Accumulates JPEG frames and finalizes a seekable RIFF/AVI segment.
 *  The writer is single-use: after successful finalization no more frames may
 *  be appended and subsequent finalization attempts fail deterministically.
 */
class AviWriter {
public:
    /** Creates a writer with immutable segment dimensions. */
    AviWriter(std::uint32_t width, std::uint32_t height);

    /** Appends one JPEG frame, adding required RIFF padding.
     *  @return False after finalization or when encoded size limits are exceeded.
     */
    bool append_frame(BytesView jpeg);

    /** Finalizes headers, `movi` size, and index into one AVI binary. */
    std::optional<ByteVector> finalize();

private:
    std::uint32_t width_;
    std::uint32_t height_;
    ByteVector data_;
    /// Internal frame location retained until the final index can be emitted.
    struct IndexEntry { std::uint32_t offset; std::uint32_t size; };
    std::vector<IndexEntry> entries_;
    bool finalized_ = false;
};

}  // namespace firmware::core
