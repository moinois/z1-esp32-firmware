/** @file @brief Implements bounded start serialization and latest-value packet replacement. */
#include "application/storage/file_transfer_admission.hpp"

#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_pending_starts = 4U;

}  // namespace

bool FileTransferAdmission::enqueue(const HostIdentity& host,
                                    core::FileTransferStart start) {
    if (pending_starts_.size() >= maximum_pending_starts) {
        return false;
    }
    pending_starts_.push_back({host, std::move(start)});
    return true;
}

std::optional<QueuedFileTransferStart> FileTransferAdmission::take_next() {
    if (active_ || pending_starts_.empty()) {
        return std::nullopt;
    }
    QueuedFileTransferStart selected = std::move(pending_starts_.front());
    pending_starts_.pop_front();
    active_ = true;
    return selected;
}

void FileTransferAdmission::finish_active() {
    active_ = false;
}

std::size_t FileTransferAdmission::pending() const {
    return pending_starts_.size();
}

bool FileTransferAdmission::active() const {
    return active_;
}

void FileTransferMailbox::put(core::Frame frame) {
    frame_ = std::move(frame);
}

std::optional<core::Frame> FileTransferMailbox::take() {
    std::optional<core::Frame> selected = std::move(frame_);
    frame_.reset();
    return selected;
}

}  // namespace firmware::application
