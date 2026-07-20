// Declares an in-memory MJPEG AVI writer for recorded video segments.
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace firmware::core {

class AviWriter {
public:
    // Creates a writer with immutable segment dimensions.
    AviWriter(std::uint32_t width, std::uint32_t height);

    // Appends one JPEG frame and returns false when the frame is too large.
    bool append_frame(BytesView jpeg);

    // Finalizes the header, movi size, and index table into one AVI binary.
    std::optional<ByteVector> finalize();

private:
    std::uint32_t width_;
    std::uint32_t height_;
    ByteVector data_;
    struct IndexEntry { std::uint32_t offset; std::uint32_t size; };
    std::vector<IndexEntry> entries_;
    bool finalized_ = false;
};

}  // namespace firmware::core
