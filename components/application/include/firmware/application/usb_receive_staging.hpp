// Declares bounded all-or-nothing staging for USB receive blocks.
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>

namespace firmware::application {

// Accumulates raw USB bytes until the transport hands them to a decoder.
class UsbReceiveStaging {
public:
    static constexpr std::size_t capacity = 32768U;

    // Appends the complete block or discards the entire staging session.
    bool stage(core::BytesView block);

    // Returns all staged bytes and resets the staging buffer.
    core::ByteVector take();

    // Clears staged bytes after disconnect or a failed block admission.
    void clear();

    // Reports the current staged byte count.
    std::size_t size() const;

private:
    core::ByteVector bytes_;
};

}  // namespace firmware::application
