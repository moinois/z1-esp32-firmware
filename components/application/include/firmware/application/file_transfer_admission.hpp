/** @file @brief Defines bounded host file-transfer start admission and packet mailbox state. */
#pragma once

#include "firmware/application/ownership.hpp"
#include "firmware/core/file_transfer_paths.hpp"
#include "firmware/core/frame.hpp"

#include <cstddef>
#include <deque>
#include <optional>

namespace firmware::application {

/// Retains the response identity and parsed request of one accepted start.
struct QueuedFileTransferStart {
    HostIdentity host;
    core::FileTransferStart start;
};

/// Serializes up to four pending starts behind one active file operation.
class FileTransferAdmission {
public:
    /// Adds a parsed start while pending capacity remains.
    bool enqueue(const HostIdentity& host, core::FileTransferStart start);

    /// Begins the oldest start only when no operation is active.
    std::optional<QueuedFileTransferStart> take_next();

    /// Opens the admission gate after the active operation terminates.
    void finish_active();

    /// Returns the number of accepted starts still waiting.
    std::size_t pending() const;

    /// Reports whether an operation currently owns the processing slot.
    bool active() const;

private:
    std::deque<QueuedFileTransferStart> pending_starts_;
    bool active_ = false;
};

/// Implements the one-frame latest-value mailbox for owner transfer packets.
class FileTransferMailbox {
public:
    /// Replaces any accepted frame not yet consumed by the operation.
    void put(core::Frame frame);

    /// Removes and returns the newest accepted frame, when present.
    std::optional<core::Frame> take();

private:
    std::optional<core::Frame> frame_;
};

}  // namespace firmware::application
