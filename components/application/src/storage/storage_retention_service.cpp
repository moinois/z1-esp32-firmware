/** @file @brief Implements periodic retention checks and deletion/refresh sequencing. */
#include "firmware/application/storage_retention_service.hpp"

namespace firmware::application {

void StorageRetentionService::run_check(StorageRetentionPort& port) {
    ++check_number_;
    const auto usage = port.read_usage();
    if (!usage.has_value()) return;
    const auto entries = port.list_video_directory();
    if (!entries.has_value()) return;
    auto decision = evaluate_storage_retention(
        usage->total_bytes, usage->free_bytes, true, check_number_, *entries);
    if (decision.request_persistence) port.request_persistence();
    if (!decision.should_prune) return;
    for (const auto& path : decision.candidates) {
        if (!port.remove_file(path)) continue;
        const auto refreshed = port.read_usage();
        if (!refreshed.has_value()) return;
        if (refreshed->total_bytes == 0U ||
            refreshed->free_bytes > refreshed->total_bytes) {
            return;
        }
        const auto used = refreshed->total_bytes - refreshed->free_bytes;
        if (used * 100U < refreshed->total_bytes * 65U) return;
    }
}

}  // namespace firmware::application
