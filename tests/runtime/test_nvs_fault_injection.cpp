// Verifies deterministic NVS failure selection without an ESP-IDF dependency.
#include "test.hpp"
#include "firmware/application/nvs_fault_injection.hpp"

TEST_CASE(nvs_fault_injection_starts_clear_and_selects_one_boundary) {
    firmware::application::NvsFaultInjection faults;

    REQUIRE(!faults.fail_open());
    REQUIRE(!faults.fail_commit());

    faults.select(firmware::application::NvsFault::open);
    REQUIRE(faults.fail_open());
    REQUIRE(!faults.fail_commit());

    faults.select(firmware::application::NvsFault::commit);
    REQUIRE(!faults.fail_open());
    REQUIRE(faults.fail_commit());

    faults.select(firmware::application::NvsFault::none);
    REQUIRE(!faults.fail_open());
    REQUIRE(!faults.fail_commit());
}
