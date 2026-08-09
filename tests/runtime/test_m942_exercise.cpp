// Tests M942 routing, scheduling, digital-I/O pattern, retries, and deadlines.
#include "test.hpp"

#include "application/runtime/m942_exercise.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using firmware::application::HostIdentity;
using firmware::application::M942ExercisePort;
using firmware::application::M942ExerciseService;
using firmware::core::ByteVector;
using firmware::core::Frame;

namespace {

// Captures exercise interactions and advances virtual time through requested delays.
class FakeM942Port final : public M942ExercisePort {
public:
    // Offers the original host frame to controller output immediately.
    void forward_to_controller(const Frame& frame) override {
        forwarded.push_back(frame);
    }

    // Sends one local response only to the originating host connection.
    void respond(const HostIdentity& host, const Frame& frame) override {
        response_hosts.push_back(host);
        responses.push_back(frame);
    }

    // Returns the current virtual monotonic time.
    std::uint64_t monotonic_milliseconds() const override {
        return now;
    }

    // Advances virtual monotonic time by the requested bounded delay.
    void delay_milliseconds(std::uint32_t duration) override {
        delays.push_back(duration);
        now += duration;
    }

    // Models exclusive SDO-client acquisition, including an unbounded wait.
    void lock_sdo_client() override {
        ++lock_count;
        now += lock_wait_milliseconds;
    }

    // Releases the shared SDO client after one exercise cycle.
    void unlock_sdo_client() override {
        ++unlock_count;
    }

    // Attempts one remote 32-bit upload under both protocol and absolute limits.
    std::optional<std::uint32_t> read_remote_u32(
        std::uint8_t node,
        std::uint16_t index,
        std::uint8_t subindex,
        std::uint32_t protocol_timeout_milliseconds,
        std::uint64_t absolute_deadline_milliseconds) override {
        read_nodes.push_back(node);
        read_indices.push_back(index);
        read_subindices.push_back(subindex);
        read_timeouts.push_back(protocol_timeout_milliseconds);
        read_deadlines.push_back(absolute_deadline_milliseconds);
        now += std::min<std::uint64_t>(
            read_attempt_milliseconds,
            absolute_deadline_milliseconds > now
                ? absolute_deadline_milliseconds - now
                : 0U);
        if (!read_results.empty()) {
            const auto result = read_results.front();
            read_results.erase(read_results.begin());
            return result;
        }
        return default_read_result;
    }

    // Attempts one remote 32-bit download under both protocol and absolute limits.
    bool write_remote_u32(
        std::uint8_t node,
        std::uint16_t index,
        std::uint8_t subindex,
        std::uint32_t value,
        std::uint32_t protocol_timeout_milliseconds,
        std::uint64_t absolute_deadline_milliseconds) override {
        write_nodes.push_back(node);
        write_indices.push_back(index);
        write_subindices.push_back(subindex);
        write_values.push_back(value);
        write_timeouts.push_back(protocol_timeout_milliseconds);
        write_deadlines.push_back(absolute_deadline_milliseconds);
        now += std::min<std::uint64_t>(
            write_attempt_milliseconds,
            absolute_deadline_milliseconds > now
                ? absolute_deadline_milliseconds - now
                : 0U);
        if (!write_results.empty()) {
            const bool result = write_results.front();
            write_results.erase(write_results.begin());
            return result;
        }
        return default_write_result;
    }

    std::uint64_t now = 1000U;
    std::uint64_t lock_wait_milliseconds = 0U;
    std::uint64_t read_attempt_milliseconds = 0U;
    std::uint64_t write_attempt_milliseconds = 0U;
    std::optional<std::uint32_t> default_read_result = 1U;
    bool default_write_result = true;
    std::vector<std::optional<std::uint32_t>> read_results;
    std::vector<bool> write_results;
    std::vector<Frame> forwarded;
    std::vector<HostIdentity> response_hosts;
    std::vector<Frame> responses;
    std::vector<std::uint32_t> delays;
    std::vector<std::uint8_t> read_nodes;
    std::vector<std::uint16_t> read_indices;
    std::vector<std::uint8_t> read_subindices;
    std::vector<std::uint32_t> read_timeouts;
    std::vector<std::uint64_t> read_deadlines;
    std::vector<std::uint8_t> write_nodes;
    std::vector<std::uint16_t> write_indices;
    std::vector<std::uint8_t> write_subindices;
    std::vector<std::uint32_t> write_values;
    std::vector<std::uint32_t> write_timeouts;
    std::vector<std::uint64_t> write_deadlines;
    std::size_t lock_count = 0U;
    std::size_t unlock_count = 0U;
};

// Converts response bytes to exact protocol text.
std::string text(const ByteVector& bytes) {
    return {bytes.begin(), bytes.end()};
}

// Creates one host general-command frame of the requested payload size.
Frame command(std::size_t size = 4U) {
    ByteVector payload(size, static_cast<std::uint8_t>('x'));
    if (size >= 4U) {
        payload[0] = 'M';
        payload[1] = '9';
        payload[2] = '4';
        payload[3] = '2';
    }
    return {0xa2U, std::move(payload)};
}

}  // namespace

TEST_CASE(can_010_every_frame_is_forwarded_but_only_admitted_bounded_frames_reply) {
    FakeM942Port port;
    M942ExerciseService service(port);
    const HostIdentity host{firmware::application::HostTransport::tcp, 7U};

    REQUIRE(service.submit(host, command(), true));
    REQUIRE_EQ(port.forwarded.size(), 1U);
    REQUIRE_EQ(port.responses.size(), 1U);
    REQUIRE_EQ(port.response_hosts[0], host);
    REQUIRE_EQ(port.responses[0].type, 0xa2U);
    REQUIRE_EQ(text(port.responses[0].payload),
               std::string("M942 ok (canopen)\n"));

    REQUIRE(!service.submit(host, command(129U), true));
    REQUIRE(!service.submit(host, command(), false));
    REQUIRE_EQ(port.forwarded.size(), 3U);
    REQUIRE_EQ(port.responses.size(), 1U);
}

TEST_CASE(can_011_one_exercise_runs_while_later_commands_still_reply) {
    FakeM942Port port;
    M942ExerciseService service(port);
    const HostIdentity host{firmware::application::HostTransport::usb, 0U};

    REQUIRE(service.submit(host, command(), true));
    REQUIRE(!service.submit(host, command(), true));
    REQUIRE(service.exercise_active());
    REQUIRE_EQ(port.responses.size(), 2U);
}

TEST_CASE(can_012_and_013_exercise_waits_then_rotates_low_input_nibble) {
    FakeM942Port port;
    port.now = 5000U;
    port.read_results = {1U, 2U, 4U, 8U};
    M942ExerciseService service(port);
    const HostIdentity host{firmware::application::HostTransport::tcp, 1U};
    service.submit(host, command(), true);

    service.run();

    REQUIRE_EQ(port.delays.front(), 500U);
    REQUIRE_EQ(port.read_nodes.front(), 1U);
    REQUIRE_EQ(port.read_indices.front(), 0x6000U);
    REQUIRE_EQ(port.read_subindices.front(), 1U);
    REQUIRE_EQ(port.write_indices.front(), 0x6001U);
    REQUIRE_EQ(port.write_values[0], 0x03U);
    REQUIRE_EQ(port.write_values[1], 0x06U);
    REQUIRE_EQ(port.write_values[2], 0x0cU);
    REQUIRE_EQ(port.write_values[3], 0x09U);
    REQUIRE_EQ(port.read_deadlines.front(), 13000U);
    REQUIRE(!service.exercise_active());
}

TEST_CASE(can_014_zero_reads_latch_all_outputs_on_tenth_success) {
    FakeM942Port port;
    port.default_read_result = 0U;
    M942ExerciseService service(port);
    const HostIdentity host{firmware::application::HostTransport::tcp, 1U};
    service.submit(host, command(), true);

    service.run();

    REQUIRE(port.write_values.size() >= 11U);
    for (std::size_t index = 0U; index < 9U; ++index) {
        REQUIRE_EQ(port.write_values[index], 0U);
    }
    REQUIRE_EQ(port.write_values[9], 0x0fU);
    REQUIRE_EQ(port.write_values[10], 0x0fU);
}

TEST_CASE(can_014_failed_reads_do_not_change_consecutive_zero_count) {
    FakeM942Port port;
    port.read_results = {0U, 0U, std::nullopt, 1U, 0U};
    M942ExerciseService service(port);
    const HostIdentity host{firmware::application::HostTransport::tcp, 1U};
    service.submit(host, command(), true);

    service.run();

    REQUIRE_EQ(port.write_values[0], 0U);
    REQUIRE_EQ(port.write_values[1], 0U);
    REQUIRE_EQ(port.write_values[2], 0x03U);
    REQUIRE_EQ(port.write_values[3], 0U);
}

TEST_CASE(can_015_each_operation_retries_35_times_with_ten_ms_gaps) {
    FakeM942Port port;
    port.default_read_result = std::nullopt;
    port.read_attempt_milliseconds = 210U;
    M942ExerciseService service(port);
    const HostIdentity host{firmware::application::HostTransport::tcp, 1U};
    service.submit(host, command(), true);

    service.run();

    REQUIRE_EQ(port.read_nodes.size(), 35U);
    REQUIRE_EQ(port.read_timeouts.front(), 800U);
    REQUIRE_EQ(port.lock_count, 1U);
    REQUIRE_EQ(port.unlock_count, 1U);
    REQUIRE_EQ(port.delays.size(), 35U);
    REQUIRE_EQ(port.delays.front(), 500U);
    for (std::size_t index = 1U; index < 35U; ++index) {
        REQUIRE_EQ(port.delays[index], 10U);
    }
}

TEST_CASE(can_015_unbounded_lock_wait_can_exhaust_absolute_deadline) {
    FakeM942Port port;
    port.lock_wait_milliseconds = 9000U;
    M942ExerciseService service(port);
    const HostIdentity host{firmware::application::HostTransport::tcp, 1U};
    service.submit(host, command(), true);

    service.run();

    REQUIRE_EQ(port.lock_count, 1U);
    REQUIRE_EQ(port.unlock_count, 1U);
    REQUIRE(port.read_nodes.empty());
}

TEST_CASE(can_015_inactive_or_already_expired_exercise_performs_no_io) {
    FakeM942Port port;
    M942ExerciseService service(port);

    service.run();
    REQUIRE(port.read_nodes.empty());

    const HostIdentity host{firmware::application::HostTransport::tcp, 1U};
    REQUIRE(service.submit(host, command(), true));
    port.now += firmware::application::m942::exercise_duration_milliseconds;
    service.run();

    REQUIRE(!service.exercise_active());
    REQUIRE(port.read_nodes.empty());
    REQUIRE(port.write_values.empty());
}

TEST_CASE(can_015_failed_writes_retry_and_stop_at_absolute_deadline) {
    FakeM942Port port;
    port.default_write_result = false;
    port.write_attempt_milliseconds = 210U;
    M942ExerciseService service(port);
    const HostIdentity host{firmware::application::HostTransport::usb, 0U};
    REQUIRE(service.submit(host, command(), true));

    service.run();

    REQUIRE_EQ(port.write_values.size(), 35U);
    REQUIRE_EQ(port.write_nodes.front(), 1U);
    REQUIRE_EQ(port.write_indices.front(), 0x6001U);
    REQUIRE_EQ(port.write_subindices.front(), 1U);
    REQUIRE_EQ(port.write_timeouts.front(), 800U);
    REQUIRE_EQ(port.write_deadlines.front(), 9000U);
    REQUIRE_EQ(port.now, 9000U);
    REQUIRE_EQ(port.lock_count, 1U);
    REQUIRE_EQ(port.unlock_count, 1U);
}

TEST_CASE(can_015_read_that_reaches_deadline_does_not_start_a_write) {
    FakeM942Port port;
    port.read_attempt_milliseconds =
        firmware::application::m942::exercise_duration_milliseconds;
    M942ExerciseService service(port);
    const HostIdentity host{firmware::application::HostTransport::tcp, 1U};
    REQUIRE(service.submit(host, command(), true));

    service.run();

    REQUIRE_EQ(port.read_nodes.size(), 1U);
    REQUIRE(port.write_values.empty());
    REQUIRE_EQ(port.unlock_count, 1U);
}
