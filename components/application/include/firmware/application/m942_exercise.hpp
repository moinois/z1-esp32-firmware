// Declares concurrent M942 command admission and remote digital-I/O exercise policy.
#pragma once

#include "firmware/application/ownership.hpp"
#include "firmware/core/frame.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace firmware::application {

namespace m942 {

inline constexpr std::size_t maximum_command_payload_size = 128U;
inline constexpr std::uint8_t response_packet_type =
    core::protocol::general_command;
inline constexpr char response_text[] = "M942 ok (canopen)\n";
inline constexpr std::uint64_t exercise_duration_milliseconds = 8000U;
inline constexpr std::uint32_t initial_delay_milliseconds = 500U;
inline constexpr std::uint32_t cycle_delay_milliseconds = 100U;
inline constexpr std::uint32_t sdo_protocol_timeout_milliseconds = 800U;
inline constexpr std::size_t maximum_sdo_attempts = 35U;
inline constexpr std::uint32_t sdo_retry_delay_milliseconds = 10U;
inline constexpr std::uint8_t remote_node_id = 1U;
inline constexpr std::uint16_t remote_input_index = 0x6000U;
inline constexpr std::uint16_t remote_output_index = 0x6001U;
inline constexpr std::uint8_t remote_io_subindex = 1U;

}  // namespace m942

// Isolates M942 policy from routing, scheduling, time, and an SDO client stack.
class M942ExercisePort {
public:
    // Enables safe destruction through a substituted exercise adapter.
    virtual ~M942ExercisePort() = default;

    // Offers the original host frame to controller output immediately.
    virtual void forward_to_controller(const core::Frame& frame) = 0;

    // Sends one local response only to the originating host connection.
    virtual void respond(const HostIdentity& host,
                         const core::Frame& frame) = 0;

    // Returns current monotonic milliseconds for absolute deadline checks.
    virtual std::uint64_t monotonic_milliseconds() const = 0;

    // Waits for a bounded policy delay.
    virtual void delay_milliseconds(std::uint32_t duration) = 0;

    // Acquires exclusive SDO client access without imposing a timeout.
    virtual void lock_sdo_client() = 0;

    // Releases exclusive SDO client access after one exercise cycle.
    virtual void unlock_sdo_client() = 0;

    // Attempts one remote 32-bit upload with protocol and absolute deadlines.
    virtual std::optional<std::uint32_t> read_remote_u32(
        std::uint8_t node,
        std::uint16_t index,
        std::uint8_t subindex,
        std::uint32_t protocol_timeout_milliseconds,
        std::uint64_t absolute_deadline_milliseconds) = 0;

    // Attempts one remote 32-bit download with protocol and absolute deadlines.
    virtual bool write_remote_u32(
        std::uint8_t node,
        std::uint16_t index,
        std::uint8_t subindex,
        std::uint32_t value,
        std::uint32_t protocol_timeout_milliseconds,
        std::uint64_t absolute_deadline_milliseconds) = 0;
};

// Owns one pending/running exercise while preserving independent forwarding.
class M942ExerciseService {
public:
    // Binds routing and exercise policy to replaceable outer operations.
    explicit M942ExerciseService(M942ExercisePort& port);

    // Forwards every command, conditionally replies, and claims one exercise.
    bool submit(const HostIdentity& host,
                const core::Frame& frame,
                bool local_capacity_accepted);

    // Runs the claimed exercise synchronously in its dedicated worker context.
    void run();

    // Reports whether an exercise is pending or running.
    bool exercise_active() const;

private:
    // Retries one input read under setup count and absolute deadline limits.
    std::optional<std::uint32_t> read_with_retries();

    // Retries one output write under setup count and absolute deadline limits.
    void write_with_retries(std::uint32_t value);

    // Produces the specified low-nibble output and updates zero-run state.
    std::uint32_t output_for_input(std::uint32_t input);

    // Waits no longer than the remaining exercise deadline.
    void delay_bounded(std::uint32_t requested_milliseconds);

    M942ExercisePort& port_;
    std::atomic_bool exercise_active_{false};
    std::uint64_t deadline_milliseconds_ = 0U;
    std::uint8_t consecutive_zero_reads_ = 0U;
};

}  // namespace firmware::application
