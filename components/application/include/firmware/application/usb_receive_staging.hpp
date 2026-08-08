/** @file @brief Bounded all-or-nothing staging for USB receive blocks. */
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>

namespace firmware::application {

/** Accumulates raw USB blocks until a worker hands them to the decoder. */
class UsbReceiveStaging {
public:
    /// Session capacity chosen to hold multiple maximum-size host frames.
    static constexpr std::size_t capacity = 32768U;

    /** Appends the complete block or discards the entire session on overflow. */
    bool stage(core::BytesView block);

    /// Transfers ownership of all staged bytes and resets the buffer.
    core::ByteVector take();

    /// Clears partial bytes after disconnect or failed all-or-nothing admission.
    void clear();

    /// Reports the current staged byte count.
    std::size_t size() const;

private:
    core::ByteVector bytes_;
};

}  // namespace firmware::application
