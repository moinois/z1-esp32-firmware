/** @file @brief Declares storage monitoring and video-file retention policy. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace firmware::application {

/** Recording candidate considered for age-ordered deletion. */
struct RetentionCandidate {
    std::string path;
    std::uint64_t modification_seconds = 0U;
    bool regular_file = false;
};

/** Selected deletion set and capacity remaining after simulated removals. */
struct RetentionDecision {
    std::uint32_t total_mebibytes = 0U;
    std::uint32_t free_mebibytes = 0U;
    bool should_prune = false;
    bool request_persistence = false;
    std::vector<std::string> candidates;
};

/// Evaluates one periodic storage check and returns ordered deletion candidates.
RetentionDecision evaluate_storage_retention(std::uint64_t total_bytes,
                                             std::uint64_t free_bytes,
                                             bool directory_open,
                                             std::uint32_t check_number,
                                             std::vector<RetentionCandidate> entries);

}  // namespace firmware::application
