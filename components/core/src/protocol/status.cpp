/** @file @brief Implements bounded generation of the host-visible machine-status extension. */
#include "firmware/core/status.hpp"

#include "firmware/core/protocol_constants.hpp"

#include <cinttypes>
#include <cstdio>

namespace firmware::core {

std::optional<std::string> extend_status(std::string_view status, const StatusExtension& extension) {
    if (status.empty() || status.front() != '<') {
        return std::nullopt;
    }
    const auto closing = status.find('>');
    if (closing == std::string_view::npos) {
        return std::nullopt;
    }

    char suffix[96];
    const int length = std::snprintf(
        suffix, sizeof(suffix), "|E:%u,%u,%u,%" PRIu32 ",%" PRIu32 "|OTA:%" PRIu32 ",%" PRIu32 ">\n",
        extension.transfer_active ? 1U : 0U, extension.record_requested ? 1U : 0U,
        extension.recording ? 1U : 0U, extension.sd_used_mib, extension.sd_total_mib,
        extension.update_phase, extension.update_progress);
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(suffix) ||
        closing + static_cast<std::size_t>(length) >
            protocol::controller_maximum_frame_size) {
        return std::nullopt;
    }

    std::string output(status.substr(0, closing));
    output.append(suffix, static_cast<std::size_t>(length));
    return output;
}

bool status_reports_running(std::string_view status) {
    return status.find("|P:") != std::string_view::npos;
}

}  // namespace firmware::core
