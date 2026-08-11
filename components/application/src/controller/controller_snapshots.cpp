/** @file @brief Implements bounded snapshot retention and byte-exact local response formatting. */
#include "application/controller/controller_snapshots.hpp"

#include "core/protocol/protocol_constants.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_snapshot_size =
    core::protocol::controller_maximum_frame_size;
constexpr std::size_t maximum_pending_statuses = 3U;
constexpr std::size_t maximum_controller_version_prefix = 63U;

constexpr std::string_view initial_status =
    "<Idle|MPos:-1.0000,-1.0000,-1.0000,0.0000,0.0000|WPos:144.4120,158.7000,77.9550,8.0010,0.0000|"
    "    F:0.0,3000.0,100.0|S:0.0,10000.0,100.0,0,27.0|T:1,-15.180|W:4.13|L:0,0,0,0.0,100.0|"
    "P:1234,50,1200|A:1|O:-1.351|H:1|C:1,5,0,1>\n";

constexpr std::string_view fallback_status =
    "<Idle|MPos:-1.0000,-1.0000,-1.0000,0.0000,0.0000|WPos:144.4120,158.7000,77.9550,8.0010,0.0000|"
    "F:0.0,3000.0,100.0|S:0.0,10000.0,100.0,0,27.0|T:1,-15.180|W:4.13|L:0,0,0,0.0,100.0|"
    "P:1234,50,1200|A:1|O:-1.351|H:1|C:1,5,0,1>\n";

// Converts retained bytes to a non-owning text view without NUL assumptions.
std::string_view as_text(const core::ByteVector& bytes) {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

// Reports whether a retained status has the minimum structural delimiters.
bool usable_status(std::string_view status) {
    return !status.empty() && status.front() == '<' && status.find('>') != std::string_view::npos;
}

}  // namespace

ControllerSnapshots::ControllerSnapshots()
    : latest_status_(initial_status.begin(), initial_status.end()) {}

core::ByteVector ControllerSnapshots::bounded_copy(core::BytesView payload) {
    const std::size_t size = std::min(payload.size(), maximum_snapshot_size);
    return {payload.begin(), payload.begin() + static_cast<std::ptrdiff_t>(size)};
}

void ControllerSnapshots::update_status(core::BytesView payload) {
    if (payload.size() == 0U) {
        return;
    }
    latest_status_ = bounded_copy(payload);
    if (pending_statuses_.size() == maximum_pending_statuses) {
        pending_statuses_.pop_front();
    }
    pending_statuses_.push_back(latest_status_);
}

void ControllerSnapshots::update_diagnostic(core::BytesView payload) {
    diagnostic_ = bounded_copy(payload);
}

void ControllerSnapshots::update_version(core::BytesView payload) {
    version_ = bounded_copy(payload);
}

std::optional<core::Frame> ControllerSnapshots::status_reply(const core::StatusExtension& extension) {
    core::ByteVector selected = latest_status_;
    if (!pending_statuses_.empty()) {
        selected = pending_statuses_.back();
        pending_statuses_.clear();
    }

    std::string_view selected_text = as_text(selected);
    if (!usable_status(selected_text)) {
        selected_text = fallback_status;
    }
    const auto extended = core::extend_status(selected_text, extension);
    if (!extended.has_value()) {
        return std::nullopt;
    }
    return core::Frame{core::protocol::machine_status,
                       {extended->begin(), extended->end()}};
}

std::optional<core::Frame> ControllerSnapshots::diagnostic_reply(std::int32_t rssi) const {
    const std::string_view diagnostic = as_text(diagnostic_);
    if (diagnostic.empty() || diagnostic.front() != '{') {
        return std::nullopt;
    }
    const std::size_t closing = diagnostic.find('}');
    if (closing == std::string_view::npos) {
        return std::nullopt;
    }

    char insertion[32];
    const int insertion_length = std::snprintf(insertion, sizeof(insertion), "|RSSI:%ld", static_cast<long>(rssi));
    const std::size_t complete_size = closing + static_cast<std::size_t>(insertion_length) + 2U;
    if (insertion_length < 0 || static_cast<std::size_t>(insertion_length) >= sizeof(insertion) ||
        complete_size > maximum_snapshot_size) {
        const std::size_t fallback_size = std::min(diagnostic.size(), closing + 2U);
        return core::Frame{
            core::protocol::diagnostic_data,
            {diagnostic_.begin(),
             diagnostic_.begin() + static_cast<std::ptrdiff_t>(fallback_size)}};
    }

    core::ByteVector payload;
    payload.reserve(complete_size);
    payload.insert(payload.end(), diagnostic_.begin(), diagnostic_.begin() + static_cast<std::ptrdiff_t>(closing));
    payload.insert(payload.end(), insertion, insertion + insertion_length);
    payload.push_back('}');
    payload.push_back('\n');
    return core::Frame{core::protocol::diagnostic_data, std::move(payload)};
}

core::Frame ControllerSnapshots::version_reply() const {
    constexpr std::string_view prefix = "version = ";
    constexpr std::string_view suffix = ".0.1.13\n";
    core::ByteVector payload(prefix.begin(), prefix.end());

    const auto nul = std::find(version_.begin(), version_.end(), 0U);
    const std::size_t controller_length = static_cast<std::size_t>(nul - version_.begin());
    if (controller_length > 0U && controller_length <= maximum_controller_version_prefix) {
        payload.insert(payload.end(), version_.begin(), nul);
    }
    payload.insert(payload.end(), suffix.begin(), suffix.end());
    return {core::protocol::text_response, std::move(payload)};
}

std::size_t ControllerSnapshots::latest_status_size() const {
    return latest_status_.size();
}

std::size_t ControllerSnapshots::diagnostic_size() const {
    return diagnostic_.size();
}

std::size_t ControllerSnapshots::version_size() const {
    return version_.size();
}

std::size_t ControllerSnapshots::pending_status_count() const {
    return pending_statuses_.size();
}

}  // namespace firmware::application
