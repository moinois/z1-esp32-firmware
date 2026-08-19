// Verifies centralized construction of trusted firmware-owned SD paths.
#include "test.hpp"

#include "core/filesystem/sd_user_path.hpp"

#include <string>

TEST_CASE(sd_owned_paths_share_one_mount_point) {
    using firmware::core::physical_sd_path;

    REQUIRE_EQ(physical_sd_path("/"), std::string("/sd"));
    REQUIRE_EQ(physical_sd_path("config.txt"), std::string("/sd/config.txt"));
    REQUIRE_EQ(physical_sd_path("/videos/a.avi"),
               std::string("/sd/videos/a.avi"));
}
