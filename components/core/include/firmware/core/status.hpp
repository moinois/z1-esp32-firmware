// Declares pure status-snapshot transformations used by command and media services.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace firmware::core {

namespace status {

// Marks both SD fields when no complete capacity sample is available.
inline constexpr std::uint32_t unavailable_sd_capacity_mib = 1234U;

}  // namespace status

struct StatusExtension {
    bool transfer_active = false;
    bool record_requested = false;
    bool recording = false;
    std::uint32_t sd_used_mib = status::unavailable_sd_capacity_mib;
    std::uint32_t sd_total_mib = status::unavailable_sd_capacity_mib;
    std::uint32_t update_phase = 0;
    std::uint32_t update_progress = 0;
};
std::optional<std::string> extend_status(std::string_view status, const StatusExtension& extension);
bool status_reports_running(std::string_view status);
}  // namespace firmware::core
