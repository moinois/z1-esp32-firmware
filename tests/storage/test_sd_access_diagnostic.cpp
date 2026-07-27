// Verifies stable SD failure reasons and mount-state precedence.
#include "test.hpp"

#include "firmware/core/sd_access_diagnostic.hpp"

#include <cerrno>
#include <string_view>

using firmware::core::sd_access_failure_reason;

TEST_CASE(sd_access_failure_reports_an_unmounted_card_before_posix_errno) {
    REQUIRE_EQ(sd_access_failure_reason(false, EACCES),
               std::string_view("SD card not mounted"));
}

TEST_CASE(sd_access_failure_maps_stable_posix_causes_when_mounted) {
    REQUIRE_EQ(sd_access_failure_reason(true, ENOENT), std::string_view("not found"));
    REQUIRE_EQ(sd_access_failure_reason(true, EACCES),
               std::string_view("permission denied"));
    REQUIRE_EQ(sd_access_failure_reason(true, EPERM),
               std::string_view("permission denied"));
    REQUIRE_EQ(sd_access_failure_reason(true, ENOTDIR),
               std::string_view("not a directory"));
    REQUIRE_EQ(sd_access_failure_reason(true, EIO), std::string_view("I/O error"));
    REQUIRE_EQ(sd_access_failure_reason(true, EINVAL),
               std::string_view("POSIX error"));
}
