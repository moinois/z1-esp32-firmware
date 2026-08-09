// Verifies persisted runtime queries and first-boot clearing commands.
#include "test.hpp"

#include "application/runtime/runtime_commands.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::RuntimeCommandPort;
using firmware::application::RuntimeCommandService;
using firmware::application::RuntimeEraseResult;
using firmware::application::RuntimeSignedRead;
using firmware::application::RuntimeValueResult;

namespace {

// Records capacity, persistent reads, UTC conversion, erase, and responses.
class FakeRuntimeCommandPort final : public RuntimeCommandPort {
public:
    // Attempts admission with the exact runtime wait duration.
    bool admit_operation(std::uint32_t wait_milliseconds) override {
        waits.push_back(wait_milliseconds);
        return admission_succeeds;
    }

    // Opens the exact runtime namespace for a persisted query.
    bool open_namespace(std::string_view name_space) override {
        namespaces.emplace_back(name_space);
        return namespace_opens;
    }

    // Returns the configured persisted first-boot read.
    RuntimeSignedRead read_first_boot(std::string_view key) override {
        keys.emplace_back(key);
        return first_boot;
    }

    // Returns one configured persisted unsigned counter by key.
    std::optional<std::uint64_t> read_counter(
        std::string_view key) override {
        keys.emplace_back(key);
        return key == "pon_s" ? power_on : machine;
    }

    // Returns the configured minute-resolution UTC rendering.
    std::optional<std::string> format_utc_minute(
        std::int64_t seconds) override {
        formatted_seconds.push_back(seconds);
        return formatted_time;
    }

    // Erases only first_boot and returns its exact result class.
    RuntimeEraseResult erase_first_boot(std::string_view name_space,
                                        std::string_view key) override {
        namespaces.emplace_back(name_space);
        keys.emplace_back(key);
        return erase_result;
    }

    // Releases one admitted runtime-operation slot.
    void complete_operation() override {
        ++completed_operations;
    }

    // Records one routed runtime response packet.
    void send_response(std::uint8_t type, std::string_view payload) override {
        response_types.push_back(type);
        responses.emplace_back(payload);
    }

    bool admission_succeeds = true;
    bool namespace_opens = true;
    RuntimeSignedRead first_boot{RuntimeValueResult::missing, 0};
    std::optional<std::uint64_t> power_on = 0U;
    std::optional<std::uint64_t> machine = 0U;
    std::optional<std::string> formatted_time = "2025/01/02 03:04";
    RuntimeEraseResult erase_result = RuntimeEraseResult::success;
    std::size_t completed_operations = 0U;
    std::vector<std::uint32_t> waits;
    std::vector<std::string> namespaces;
    std::vector<std::string> keys;
    std::vector<std::int64_t> formatted_seconds;
    std::vector<std::uint8_t> response_types;
    std::vector<std::string> responses;
};

}  // namespace

TEST_CASE(run_040_sys_time_validates_trailing_whitespace_before_capacity) {
    FakeRuntimeCommandPort port;
    RuntimeCommandService runtime(port);

    runtime.handle_system_time("sys-time x");

    REQUIRE(port.waits.empty());
    REQUIRE_EQ(port.responses,
               std::vector<std::string>({"The command format is invalid\n"}));
}

TEST_CASE(run_040_sys_time_capacity_saturation_has_exact_response) {
    FakeRuntimeCommandPort port;
    port.admission_succeeds = false;
    RuntimeCommandService runtime(port);

    runtime.handle_system_time("sys-time\t\r\n");

    REQUIRE_EQ(port.waits, std::vector<std::uint32_t>({200U}));
    REQUIRE_EQ(port.responses,
               std::vector<std::string>({"sys-time: busy\n"}));
}

TEST_CASE(run_041_to_042_sys_time_reads_persisted_values_and_formats_utc) {
    FakeRuntimeCommandPort port;
    port.first_boot = {RuntimeValueResult::success, 1700000000};
    port.power_on = 123U;
    port.machine = 45U;
    RuntimeCommandService runtime(port);

    runtime.handle_system_time("sys-time ");

    REQUIRE_EQ(port.namespaces, std::vector<std::string>({"runtime"}));
    REQUIRE_EQ(port.keys,
               std::vector<std::string>({"first_boot", "pon_s", "mach_s"}));
    REQUIRE_EQ(port.responses,
               std::vector<std::string>({
                   "sys-time-data = 2025/01/02 03:04,123,45\n"}));
    REQUIRE_EQ(port.completed_operations, 1U);
    REQUIRE_EQ(port.response_types, std::vector<std::uint8_t>({0x83U}));
}

TEST_CASE(run_041_to_042_missing_or_unreadable_values_use_required_fallbacks) {
    FakeRuntimeCommandPort port;
    port.first_boot = {RuntimeValueResult::success, 1700000000};
    port.formatted_time = std::nullopt;
    port.power_on = std::nullopt;
    port.machine = std::nullopt;
    RuntimeCommandService runtime(port);

    runtime.handle_system_time("sys-time");
    REQUIRE_EQ(port.responses,
               std::vector<std::string>({
                   "sys-time-data = invalid,0,0\n"}));

    port.responses.clear();
    port.first_boot = {RuntimeValueResult::missing, 0};
    runtime.handle_system_time("sys-time");
    REQUIRE_EQ(port.responses,
               std::vector<std::string>({
                   "sys-time-data = null,0,0\n"}));
}

TEST_CASE(run_042_namespace_failure_sends_failure_then_null_result) {
    FakeRuntimeCommandPort port;
    port.namespace_opens = false;
    RuntimeCommandService runtime(port);

    runtime.handle_system_time("sys-time");

    REQUIRE_EQ(port.responses,
               std::vector<std::string>({"sys-time-data get failed\n",
                                         "sys-time-data = null,0,0\n"}));
    REQUIRE_EQ(port.completed_operations, 1U);
}

TEST_CASE(run_043_clearftm_validates_capacity_and_erases_only_first_boot) {
    FakeRuntimeCommandPort port;
    RuntimeCommandService runtime(port);

    runtime.handle_clear_first_boot("clearftm x");
    REQUIRE_EQ(port.responses.back(),
               std::string("The command format is invalid\n"));
    port.admission_succeeds = false;
    runtime.handle_clear_first_boot("clearftm");
    REQUIRE_EQ(port.responses.back(), std::string("clearftm: busy\n"));

    port.admission_succeeds = true;
    runtime.handle_clear_first_boot("clearftm\n");
    REQUIRE_EQ(port.namespaces.back(), std::string("runtime"));
    REQUIRE_EQ(port.keys.back(), std::string("first_boot"));
    REQUIRE_EQ(port.responses.back(), std::string("clearftm ok\n"));
}

TEST_CASE(run_043_missing_clear_key_is_success_but_other_failure_is_not) {
    FakeRuntimeCommandPort port;
    RuntimeCommandService runtime(port);

    port.erase_result = RuntimeEraseResult::missing;
    runtime.handle_clear_first_boot("clearftm");
    port.erase_result = RuntimeEraseResult::failure;
    runtime.handle_clear_first_boot("clearftm");

    REQUIRE_EQ(port.responses,
               std::vector<std::string>({"clearftm ok\n",
                                         "clearftm failed\n"}));
}
