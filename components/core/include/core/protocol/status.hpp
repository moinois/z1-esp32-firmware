/** @file @brief Pure status transformations used by command and media services. */
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace firmware::core {

namespace status {

/// Protocol sentinel used for both SD fields when capacity cannot be sampled.
inline constexpr std::uint32_t unavailable_sd_capacity_mib = 1234U;

}  // namespace status

/** Runtime-owned fields appended to a valid controller status snapshot. */
struct StatusExtension {
    /// Whether either host transport currently owns a file transfer.
    bool transfer_active = false;
    /// Whether recording was requested, even if conditions prevent capture.
    bool record_requested = false;
    /// Whether media capture is actively writing a recording.
    bool recording = false;
    /// Used SD capacity in MiB or @ref status::unavailable_sd_capacity_mib.
    std::uint32_t sd_used_mib = status::unavailable_sd_capacity_mib;
    /// Total SD capacity in MiB or @ref status::unavailable_sd_capacity_mib.
    std::uint32_t sd_total_mib = status::unavailable_sd_capacity_mib;
    /// Persistent aggregate-update phase exposed to host diagnostics.
    std::uint32_t update_phase = 0;
    /// Progress value associated with the active update phase.
    std::uint32_t update_progress = 0;
};

/** Appends runtime fields to a syntactically valid controller status record.
 *  @return Extended record, or no value if the controller input is malformed.
 */
std::optional<std::string> extend_status(std::string_view status, const StatusExtension& extension);
/** Reports whether the controller status indicates a running machine state. */
bool status_reports_running(std::string_view status);
}  // namespace firmware::core
