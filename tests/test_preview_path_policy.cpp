// Verifies the exact allowed preview directory and traversal rejection.
#include "test.hpp"

#include "firmware/core/preview_path_policy.hpp"

using firmware::core::preview_path_allowed;

TEST_CASE(prev_010_only_sd_videos_root_and_descendants_are_allowed) {
    REQUIRE(preview_path_allowed("/sd/videos"));
    REQUIRE(preview_path_allowed("/sd/videos/clip.avi"));
    REQUIRE(preview_path_allowed("/sd/videos/nested/clip.avi"));
}

TEST_CASE(prev_010_other_roots_and_any_parent_marker_are_rejected) {
    REQUIRE(!preview_path_allowed("/sd/video"));
    REQUIRE(!preview_path_allowed("/sd/videos2/clip.avi"));
    REQUIRE(!preview_path_allowed("/sd/videos/../secret.avi"));
    REQUIRE(!preview_path_allowed("/sd/videos/a..b.avi"));
    REQUIRE(!preview_path_allowed("sd/videos/clip.avi"));
}
