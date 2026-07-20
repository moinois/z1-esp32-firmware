// Declares deterministic CANopen node identity, NMT, and heartbeat behavior.
#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace firmware::core {

namespace canopen {

inline constexpr std::uint8_t node_id = 0x11U;
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
inline constexpr bool block_transfer_enabled = false;

}  // namespace canopen

// Holds one bounded classic CAN frame without depending on a TWAI driver type.
struct CanFrame {
    std::uint16_t identifier = 0U;
    std::array<std::uint8_t, 8U> data{};
    std::uint8_t size = 0U;
};

// Uses the standard one-byte CANopen heartbeat state representation.
enum class NmtState : std::uint8_t {
    initializing = 0U,
    stopped = 4U,
    operational = 5U,
    pre_operational = 127U,
};

// Reports the externally visible work produced by one 10 ms node cycle.
struct CanopenCycleResult {
    std::optional<CanFrame> frame;
    bool restart_mainboard = false;
};

// Owns local NMT state and producer-heartbeat timing independently of TWAI.
class CanopenNode {
public:
    // Creates an initializing node with one boot-up frame pending.
    CanopenNode() = default;

    // Applies one valid local or broadcast NMT request and ignores other frames.
    void accept_nmt(const CanFrame& frame);

    // Applies an in-memory write to producer-heartbeat object 0x1017.
    void set_producer_heartbeat_period(std::uint16_t period_milliseconds);

    // Applies error-register transition policy to the local NMT state.
    void set_error_register(std::uint8_t error_register);

    // Advances one exact 10 ms processing cycle and returns its output action.
    CanopenCycleResult process_cycle();

    // Returns the current state encoded by subsequent heartbeat frames.
    NmtState state() const;

    // Returns the retained in-memory producer-heartbeat period.
    std::uint16_t producer_heartbeat_period() const;

private:
    // Reinitializes communication while retaining dictionary configuration.
    void reset_communication();

    // Changes NMT state and schedules immediate publication when enabled.
    void select_state(NmtState state);

    // Creates the one-byte heartbeat or boot-up frame.
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
