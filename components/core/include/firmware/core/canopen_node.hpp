/** @file @brief Deterministic CANopen identity, NMT, and heartbeat behavior. */
#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace firmware::core {

namespace canopen {

/// Fixed CANopen node ID assigned to the mainboard.
inline constexpr std::uint8_t node_id = 0x11U;
/// Broadcast Network Management identifier.
inline constexpr std::uint16_t nmt_identifier = 0x000U;
inline constexpr std::uint16_t sdo_request_identifier = 0x600U + node_id;
inline constexpr std::uint16_t sdo_response_identifier = 0x580U + node_id;
inline constexpr std::uint16_t heartbeat_identifier = 0x700U + node_id;
inline constexpr std::uint16_t initial_heartbeat_period_milliseconds = 0U;
inline constexpr std::uint16_t first_heartbeat_limit_milliseconds = 500U;
inline constexpr std::uint16_t processing_period_milliseconds = 10U;
inline constexpr std::uint16_t restart_delay_milliseconds = 100U;
inline constexpr std::uint16_t sdo_server_timeout_milliseconds = 2000U;
inline constexpr std::uint16_t sdo_client_timeout_milliseconds = 500U;
/// Payload capacity of a classic CAN data frame.
inline constexpr std::size_t classic_frame_capacity = 8U;
inline constexpr bool block_transfer_enabled = false;

}  // namespace canopen

/** Bounded classic CAN frame independent of the ESP-IDF TWAI type. */
struct CanFrame {
    /// Standard 11-bit CAN identifier stored in a target-neutral integer.
    std::uint16_t identifier = 0U;
    /// Fixed classic-CAN payload capacity.
    std::array<std::uint8_t, canopen::classic_frame_capacity> data{};
    /// Number of meaningful payload bytes, from zero through eight.
    std::uint8_t size = 0U;
};

/** Standard one-byte CANopen heartbeat state representation. */
enum class NmtState : std::uint8_t {
    initializing = 0U,
    stopped = 4U,
    operational = 5U,
    pre_operational = 127U,
};

/** Externally visible work produced by one exact node-processing cycle. */
struct CanopenCycleResult {
    /// Optional boot-up or heartbeat frame due during this cycle.
    std::optional<CanFrame> frame;
    /// Requests a delayed target restart after the NMT reset-node command.
    bool restart_mainboard = false;
};

/** Service-level effect selected while accepting an NMT request. */
enum class NmtRequestEffect {
    none,
    communication_reset,
    restart_scheduled,
};

/** Owns local NMT state and heartbeat timing independently of TWAI. */
class CanopenNode {
public:
    /// Creates an initializing node with one boot-up frame pending.
    CanopenNode() = default;

    /** Applies a valid local/broadcast NMT request and ignores other frames. */
    NmtRequestEffect accept_nmt(const CanFrame& frame);

    /// Applies an in-memory write to producer-heartbeat object 0x1017.
    void set_producer_heartbeat_period(std::uint16_t period_milliseconds);

    /// Applies error-register transition policy to the local NMT state.
    void set_error_register(std::uint8_t error_register);

    /// Advances one exact processing cycle and returns its output action.
    CanopenCycleResult process_cycle();

    /// Returns the state encoded by subsequent heartbeat frames.
    NmtState state() const;

    /// Returns the retained in-memory producer-heartbeat period.
    std::uint16_t producer_heartbeat_period() const;

private:
    /// Reinitializes communication while retaining dictionary configuration.
    void reset_communication();

    /// Changes NMT state and schedules immediate publication when enabled.
    void select_state(NmtState state);

    /// Creates the one-byte heartbeat or boot-up frame.
    static CanFrame heartbeat_frame(NmtState state);

    NmtState state_ = NmtState::initializing;
    std::uint16_t producer_period_milliseconds_ =
        canopen::initial_heartbeat_period_milliseconds;
    std::uint16_t heartbeat_remaining_milliseconds_ = 0U;
    std::uint16_t restart_remaining_milliseconds_ = 0U;
    bool bootup_pending_ = true;
    bool heartbeat_forced_ = false;
};

}  // namespace firmware::core
