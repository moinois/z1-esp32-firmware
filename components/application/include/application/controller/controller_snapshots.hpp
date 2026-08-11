/** @file @brief Stores bounded controller snapshots and creates local host response frames. */
#pragma once

#include "core/protocol/frame.hpp"
#include "core/protocol/status.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace firmware::application {

/** Retains latest controller status, diagnostics, and version atomically by type. */
class ControllerSnapshots {
public:
    /// Creates the store with the specified initial status sample.
    ControllerSnapshots();

    /// Replaces the latest nonempty status and queues it for one pending request.
    void update_status(core::BytesView payload);

    /// Replaces a nonempty diagnostic prefix while retaining the older suffix.
    void update_diagnostic(core::BytesView payload);

    /// Replaces a nonempty version prefix while retaining the older suffix.
    void update_version(core::BytesView payload);

    /// Consumes pending status state and creates the extended status response.
    std::optional<core::Frame> status_reply(const core::StatusExtension& extension);

    /// Creates a diagnostic response when the retained syntax is usable.
    std::optional<core::Frame> diagnostic_reply(std::int32_t rssi) const;

    /// Creates the fixed mainboard version response with an optional controller prefix.
    core::Frame version_reply() const;

    /// Exposes retained sizes for capacity tests and diagnostic reporting.
    std::size_t latest_status_size() const;
    std::size_t diagnostic_size() const;
    std::size_t version_size() const;
    std::size_t pending_status_count() const;

private:
    static core::ByteVector bounded_status_copy(core::BytesView payload);
    static void replace_text_prefix(core::ByteVector& retained,
                                    core::BytesView payload);

    core::ByteVector latest_status_;
    std::deque<core::ByteVector> pending_statuses_;
    core::ByteVector diagnostic_;
    core::ByteVector version_;
};

}  // namespace firmware::application
