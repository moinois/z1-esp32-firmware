/** @file @brief Defines transport-independent controller UART activity and output policies. */
#pragma once

#include "firmware/core/bytes.hpp"
#include "firmware/core/frame.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string_view>
#include <vector>

namespace firmware::application {

/// Describes one diagnostic message without owning static text storage.
struct DiagnosticMessage {
    std::string_view tag;
    std::string_view message;
};

/// Tracks structurally valid controller frames and reports prolonged inactivity.
class ControllerActivityMonitor {
public:
    /// Starts the first inactivity period at the supplied monotonic time.
    explicit ControllerActivityMonitor(std::uint64_t start_milliseconds);

    /// Restarts the inactivity period after any structurally valid frame.
    void record_valid_frame(std::uint64_t now_milliseconds);

    /// Returns one synthetic console alarm for every elapsed inactivity period.
    std::vector<core::Frame> poll(std::uint64_t now_milliseconds);

private:
    std::uint64_t next_alarm_milliseconds_;
};

/// Owns the bounded FIFO and minimum spacing for controller UART writes.
class ControllerOutputQueue {
public:
    /// Adds one complete encoded frame when its size and queue capacity permit.
    bool enqueue(core::ByteVector item);

    /// Removes the next frame when a UART write may be initiated.
    std::optional<core::ByteVector> take_ready(std::uint64_t now_milliseconds);

    /// Maps a completed UART write result to its required diagnostic, if any.
    std::optional<DiagnosticMessage> record_write_result(int bytes_written) const;

    /// Returns the number of complete frames waiting to be written.
    std::size_t pending() const;

private:
    std::deque<core::ByteVector> items_;
    std::uint64_t next_write_milliseconds_ = 0U;
};

}  // namespace firmware::application
