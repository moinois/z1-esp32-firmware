// Verifies first-boot, power-on, and streamed-play runtime counter policy.
#include "test.hpp"

#include "application/runtime/runtime_counters.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::FirstBootRead;
using firmware::application::FirstBootReadResult;
using firmware::application::RuntimeCounterPort;
using firmware::application::RuntimeCounterService;

namespace {

// Records exact runtime namespace/key reads and silent persistence attempts.
class FakeRuntimeCounterPort final : public RuntimeCounterPort {
public:
    // Returns the configured first-boot read result.
    FirstBootRead read_first_boot(std::string_view name_space,
                                  std::string_view key) override {
        read_namespaces.emplace_back(name_space);
        read_keys.emplace_back(key);
        return first_boot_read;
    }

    // Attempts to persist the first successful time-sync seconds.
    bool write_first_boot(std::string_view name_space, std::string_view key,
                          std::int64_t seconds) override {
        write_namespaces.emplace_back(name_space);
        write_keys.emplace_back(key);
        first_boot_writes.push_back(seconds);
        return writes_succeed;
    }

    // Returns a configured unsigned persisted counter by exact key.
    std::optional<std::uint64_t> read_counter(
        std::string_view name_space, std::string_view key) override {
        read_namespaces.emplace_back(name_space);
        read_keys.emplace_back(key);
        if (key == "pon_s") {
            return power_on_read;
        }
        return machine_read;
    }

    // Attempts one silent unsigned counter persistence update.
    bool write_counter(std::string_view name_space, std::string_view key,
                       std::uint64_t value) override {
        write_namespaces.emplace_back(name_space);
        write_keys.emplace_back(key);
        if (key == "pon_s") {
            power_on_writes.push_back(value);
        } else {
            machine_writes.push_back(value);
        }
        return writes_succeed;
    }

    bool writes_succeed = true;
    FirstBootRead first_boot_read{FirstBootReadResult::missing, 0};
    std::optional<std::uint64_t> power_on_read;
    std::optional<std::uint64_t> machine_read;
    std::vector<std::string> read_namespaces;
    std::vector<std::string> read_keys;
    std::vector<std::string> write_namespaces;
    std::vector<std::string> write_keys;
    std::vector<std::int64_t> first_boot_writes;
    std::vector<std::uint64_t> power_on_writes;
    std::vector<std::uint64_t> machine_writes;
};

}  // namespace

TEST_CASE(run_030_and_032_initialization_loads_counters_or_defaults_zero) {
    FakeRuntimeCounterPort port;
    port.power_on_read = 10U;
    RuntimeCounterService counters(port);

    counters.initialize(100U);

    REQUIRE_EQ(counters.power_on_seconds(), 10U);
    REQUIRE_EQ(counters.machine_seconds(), 0U);
    REQUIRE_EQ(port.read_namespaces,
               std::vector<std::string>({"runtime", "runtime"}));
    REQUIRE_EQ(port.read_keys,
               std::vector<std::string>({"pon_s", "mach_s"}));
}

TEST_CASE(run_031_first_boot_is_written_only_when_key_is_absent) {
    FakeRuntimeCounterPort port;
    RuntimeCounterService counters(port);

    counters.record_first_boot(1700000000);
    REQUIRE_EQ(port.first_boot_writes,
               std::vector<std::int64_t>({1700000000}));

    port.first_boot_read = {FirstBootReadResult::present, 1};
    counters.record_first_boot(1800000000);
    port.first_boot_read = {FirstBootReadResult::failure, 0};
    counters.record_first_boot(1900000000);
    REQUIRE_EQ(port.first_boot_writes.size(), 1U);
    REQUIRE_EQ(port.write_namespaces[0], std::string("runtime"));
    REQUIRE_EQ(port.write_keys[0], std::string("first_boot"));
}

TEST_CASE(run_032_power_on_saves_discard_fractional_seconds_each_time) {
    FakeRuntimeCounterPort port;
    port.power_on_read = 5U;
    RuntimeCounterService counters(port);
    counters.initialize(100U);

    counters.save_power_on(1600U);
    counters.save_power_on(2500U);
    counters.save_power_on(3600U);

    REQUIRE_EQ(port.power_on_writes,
               std::vector<std::uint64_t>({6U, 6U, 7U}));
    REQUIRE_EQ(counters.power_on_seconds(), 7U);
}

TEST_CASE(run_033_machine_time_uses_running_intervals_and_ignores_redundancy) {
    FakeRuntimeCounterPort port;
    port.machine_read = 10U;
    RuntimeCounterService counters(port);
    counters.initialize(0U);

    counters.play_running_changed(true, 100U);
    counters.play_running_changed(true, 900U);
    counters.play_running_changed(false, 2600U);
    counters.play_running_changed(false, 3700U);

    REQUIRE_EQ(port.machine_writes, std::vector<std::uint64_t>({12U}));
    REQUIRE_EQ(counters.machine_seconds(), 12U);
}

TEST_CASE(run_034_every_play_notification_also_saves_power_on_seconds) {
    FakeRuntimeCounterPort port;
    RuntimeCounterService counters(port);
    counters.initialize(0U);

    counters.play_running_changed(false, 1200U);
    counters.play_running_changed(true, 2500U);
    counters.play_running_changed(false, 4900U);

    REQUIRE_EQ(port.power_on_writes,
               std::vector<std::uint64_t>({1U, 2U, 4U}));
    REQUIRE_EQ(port.machine_writes, std::vector<std::uint64_t>({2U}));
}

TEST_CASE(run_035_persistence_failures_do_not_change_counter_flow) {
    FakeRuntimeCounterPort port;
    port.writes_succeed = false;
    RuntimeCounterService counters(port);
    counters.initialize(0U);

    counters.play_running_changed(true, 0U);
    counters.play_running_changed(false, 2500U);

    REQUIRE_EQ(counters.power_on_seconds(), 2U);
    REQUIRE_EQ(counters.machine_seconds(), 2U);
}
