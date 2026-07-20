// Verifies storage thresholds, candidate filtering, ordering, and persistence cadence.
#include "test.hpp"
#include "firmware/application/storage_retention.hpp"

TEST_CASE(rec_020_to_024_retention_filters_and_sorts_video_candidates) {
    std::vector<firmware::application::RetentionCandidate> entries{
        {"/sd/videos/new.MP4", 20U, true},
        {"/sd/videos/old.avi", 2U, true},
        {"/sd/videos/no.txt", 1U, true},
        {"/sd/videos/dir.mp4", 0U, false},
    };
    const auto decision = firmware::application::evaluate_storage_retention(
        100U * 1048576U, 35U * 1048576U, true, 60U, std::move(entries));
    REQUIRE(decision.should_prune);
    REQUIRE(decision.request_persistence);
    REQUIRE_EQ(decision.total_mebibytes, 100U);
    REQUIRE_EQ(decision.free_mebibytes, 35U);
    REQUIRE_EQ(decision.candidates, std::vector<std::string>({
        "/sd/videos/old.avi", "/sd/videos/new.MP4"}));
}

TEST_CASE(rec_024_storage_failures_skip_pruning) {
    const auto failed = firmware::application::evaluate_storage_retention(
        0U, 0U, false, 1U, {});
    REQUIRE(!failed.should_prune);
    const auto low = firmware::application::evaluate_storage_retention(
        100U, 50U, true, 2U, {});
    REQUIRE(!low.should_prune);
}
