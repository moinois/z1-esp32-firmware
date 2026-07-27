// Verifies that every user-controlled path remains inside the physical SD root.
#include "test.hpp"

#include "firmware/core/sd_user_path.hpp"

#include <string>

using firmware::core::resolve_sd_user_path;

TEST_CASE(sd_paths_share_one_mount_point_and_hide_it_from_user_output) {
    using firmware::core::logical_sd_path;
    using firmware::core::physical_sd_path;

    REQUIRE_EQ(physical_sd_path("/"), std::string("/sd"));
    REQUIRE_EQ(physical_sd_path("config.txt"), std::string("/sd/config.txt"));
    REQUIRE_EQ(logical_sd_path("/sd"), std::string("/"));
    REQUIRE_EQ(logical_sd_path("/sd/config.txt"), std::string("/config.txt"));
    REQUIRE_EQ(logical_sd_path("/other/file"), std::string("/other/file"));
}

TEST_CASE(sd_user_path_maps_logical_root_and_relative_paths_beneath_sd) {
    REQUIRE_EQ(resolve_sd_user_path("/"), std::string("/sd"));
    REQUIRE_EQ(resolve_sd_user_path("."), std::string("/sd"));
    REQUIRE_EQ(resolve_sd_user_path("recordings/file.avi"),
               std::string("/sd/recordings/file.avi"));
    REQUIRE_EQ(resolve_sd_user_path("/recordings/file.avi"),
               std::string("/sd/recordings/file.avi"));
}

TEST_CASE(sd_user_path_retains_the_existing_sd_prefix_as_a_compatibility_alias) {
    REQUIRE_EQ(resolve_sd_user_path("/sd"), std::string("/sd"));
    REQUIRE_EQ(resolve_sd_user_path("/sd/config.txt"),
               std::string("/sd/config.txt"));
}

TEST_CASE(sd_user_path_cannot_escape_or_select_another_vfs_mount) {
    REQUIRE_EQ(resolve_sd_user_path("../../spiffs/index.html"),
               std::string("/sd/spiffs/index.html"));
    REQUIRE_EQ(resolve_sd_user_path("/spiffs/index.html"),
               std::string("/sd/spiffs/index.html"));
    REQUIRE_EQ(resolve_sd_user_path("/sd/../../secret"),
               std::string("/sd/secret"));
}

TEST_CASE(sd_user_path_maps_only_an_exact_gcodes_component_to_the_canonical_tree) {
    REQUIRE_EQ(resolve_sd_user_path("/gcodes/job.nc"),
               std::string("/sd/gcodes/job.nc"));
    REQUIRE_EQ(resolve_sd_user_path("/hello/gcodes/project/job.nc"),
               std::string("/sd/gcodes/project/job.nc"));
    REQUIRE_EQ(resolve_sd_user_path("\\hello\\gcodes\\job.nc"),
               std::string("/sd/gcodes/job.nc"));
    REQUIRE_EQ(resolve_sd_user_path("/MyFilegcodes/text.txt"),
               std::string("/sd/MyFilegcodes/text.txt"));
    REQUIRE_EQ(resolve_sd_user_path("/gcodes_backup/text.txt"),
               std::string("/sd/gcodes_backup/text.txt"));
}
