// Verifies exact DIAG-028 POSIX filesystem diagnostic records.
#include "test.hpp"

#include "application/diagnostics/filesystem_diagnostics.hpp"

#include <string>

using namespace firmware::application;

TEST_CASE(diag_028_formats_posix_filesystem_failures_exactly) {
    REQUIRE_EQ(filesystem_opendir_failure("/sd/jobs", 19),
               std::string("opendir /sd/jobs failed: errno=19"));
    REQUIRE_EQ(filesystem_mkdir_failure("/sd/jobs", 13),
               std::string("mkdir failed: /sd/jobs errno=13"));
    REQUIRE_EQ(filesystem_remove_failure("/sd/jobs", -1, 39),
               std::string("rm failed: /sd/jobs result=-1 errno=39"));
    REQUIRE_EQ(filesystem_rename_failure("/sd/old", "/sd/new", 18),
               std::string("rename /sd/old -> /sd/new failed: errno=18"));
}
