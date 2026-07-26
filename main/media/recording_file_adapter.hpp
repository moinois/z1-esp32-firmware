// Declares the replaceable FAT/POSIX writer for finalized AVI segments.
#pragma once

#include "firmware/core/bytes.hpp"

#include <string_view>

namespace firmware::target {

// Writes one complete finalized recording segment to the mounted SD volume.
class RecordingFileAdapter {
public:
    // Performs bounded short-write handling, flush, and durable close.
    bool write_segment(std::string_view path, firmware::core::BytesView data) const;
};

}  // namespace firmware::target
