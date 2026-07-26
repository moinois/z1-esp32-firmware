// Implements bounded storage checks, case-insensitive video suffix filtering, and pruning order.
#include "firmware/application/storage_retention.hpp"

#include <algorithm>

namespace firmware::application {
namespace {

bool video_extension(std::string_view path) {
    const auto slash = path.find_last_of('/');
    const auto dot = path.find_last_of('.');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) return false;
    const auto extension = path.substr(dot);
    if (extension.size() != 4U) return false;
    const bool avi = (extension[1] == 'a' || extension[1] == 'A') &&
                     (extension[2] == 'v' || extension[2] == 'V') &&
                     (extension[3] == 'i' || extension[3] == 'I');
    const bool mp4 = (extension[1] == 'm' || extension[1] == 'M') &&
                    (extension[2] == 'p' || extension[2] == 'P') &&
                    extension[3] == '4';
    return avi || mp4;
}

}  // namespace

RetentionDecision evaluate_storage_retention(std::uint64_t total_bytes,
                                             std::uint64_t free_bytes,
                                             bool directory_open,
                                             std::uint32_t check_number,
                                             std::vector<RetentionCandidate> entries) {
    RetentionDecision decision;
    decision.request_persistence = check_number != 0U && check_number % 60U == 0U;
    decision.total_mebibytes = static_cast<std::uint32_t>(total_bytes / 1048576U);
    decision.free_mebibytes = static_cast<std::uint32_t>(free_bytes / 1048576U);
    if (!directory_open || total_bytes == 0U || free_bytes > total_bytes) return decision;
    const std::uint64_t used = total_bytes - free_bytes;
    if (used * 100U < total_bytes * 65U) return decision;
    decision.should_prune = true;
    entries.erase(std::remove_if(entries.begin(), entries.end(), [](const auto& entry) {
        return !entry.regular_file || entry.path.size() >= 256U || !video_extension(entry.path);
    }), entries.end());
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        return left.modification_seconds < right.modification_seconds;
    });
    for (const auto& entry : entries) decision.candidates.push_back(entry.path);
    return decision;
}

}  // namespace firmware::application
