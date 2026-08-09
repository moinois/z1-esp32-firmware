/** @file @brief Declares a deterministic protocol-level controller UART simulation. */
#pragma once

#include "controller_channel_adapter.hpp"
#include "core/protocol/frame.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace firmware::target {

/// Converts normal mainboard queries into deterministic encoded controller replies.
class MockControllerChannelAdapter final : public ControllerChannelAdapter {
public:
    /// Initializes the decoder and queues controller identity once.
    bool initialize() override;

    /// Returns queued response bytes with the normal UART read bound.
    int read(std::uint8_t* destination, std::size_t capacity) override;

    /// Accepts one encoded frame and generates any corresponding mock reply.
    int write(firmware::core::BytesView frame) override;

private:
/** Controller resource selected by one mock transfer session. */
enum class TransferKind {
        none,
        firmware,
        configuration,
        factory,
    };

/** Deterministic protocol failure injected into the next mock transfer. */
enum class TransferFault {
        none,
        malformed_geometry,
        timeout,
        controller_cancel,
    };

    /// Queues one response using the production frame encoder.
    void queue_response(firmware::core::Frame frame);

    /// Interprets one frame written by the mainboard and advances simulations.
    void handle_frame(const firmware::core::Frame& frame);

    /// Begins one controller-originated transfer selected by a test command.
    void start_transfer(TransferKind kind,
                        TransferFault fault = TransferFault::none);

    /// Consumes one mainboard transfer response and queues the next request.
    void handle_transfer_response(const firmware::core::Frame& frame);

    /// Queues a one-based data request for the active transfer.
    void request_transfer_data();

    /// Completes the active transfer and publishes its diagnostic result.
    void complete_transfer(bool succeeded);

    /// Delays one controller frame without blocking the production UART task.
    void schedule_response(firmware::core::Frame frame,
                           std::uint64_t delay_milliseconds);

    firmware::core::StreamDecoder decoder_{
        firmware::core::StreamPolicy::controller_uart()};
    std::deque<std::uint8_t> pending_input_;
    std::optional<firmware::core::Frame> delayed_response_;
    std::int64_t delayed_response_due_microseconds_ = 0;
    std::string diagnostic_;
    std::string pending_command_name_;
    TransferKind transfer_kind_ = TransferKind::none;
    TransferFault transfer_fault_ = TransferFault::none;
    std::uint8_t transfer_family_ = 0U;
    std::uint32_t transfer_frame_count_ = 0U;
    std::uint32_t transfer_index_ = 0U;
    bool initialized_ = false;
};

}  // namespace firmware::target
