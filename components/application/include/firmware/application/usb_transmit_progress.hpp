// Declares the USB transmit no-progress timeout policy.
#pragma once

#include <cstdint>

namespace firmware::application {

// Tracks progress for one USB frame without owning the transport buffer.
class UsbTransmitProgress {
public:
    static constexpr std::uint32_t no_progress_timeout_milliseconds = 500U;

    // Starts tracking a newly submitted frame at the given monotonic time.
    void begin(std::uint64_t monotonic_milliseconds);

    // Records any positive write progress and restarts the timeout window.
    void record_progress(std::uint64_t monotonic_milliseconds);

    // Reports whether the unsent remainder must be discarded.
    bool expired(std::uint64_t monotonic_milliseconds) const;

    // Stops tracking the current frame after completion or discard.
    void clear();

private:
    bool active_ = false;
    std::uint64_t last_progress_milliseconds_ = 0U;
};

}  // namespace firmware::application
