/** @file @brief Defines the controller firmware-transfer state machine and its replaceable port. */
#pragma once

#include "core/protocol/frame.hpp"
#include "application/diagnostics/controller_diagnostics.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {

/** Controller firmware-transfer event emitted by validated protocol input. */
enum class FirmwareTransferEvent {
    started,
    progress,
    error,
    completed,
    cancelled,
    timed_out,
};

/// Isolates firmware-transfer policy from storage, transport, and status sinks.
class ControllerFirmwarePort {
public:
    /// Enables safe destruction through a substituted port implementation.
    virtual ~ControllerFirmwarePort() = default;

    /// Reports whether the requested firmware file is currently available.
    virtual bool file_exists(std::string_view path) = 0;

    /// Returns the current firmware file size or failure.
    virtual std::optional<std::uint64_t> file_size(std::string_view path) = 0;

    /// Triggers the target's fatal zero-size layout behavior.
    virtual void panic_on_zero_frame_size() = 0;

    /// Reopens and reads at most one negotiated block from the requested offset.
    virtual std::optional<core::ByteVector> read_file(std::string_view path,
                                                      std::uint64_t offset,
                                                      std::size_t maximum_size) = 0;

    /// Probes transient data-response retention for deterministic DIAG-035 handling.
    virtual bool response_data_memory_available(std::size_t bytes) {
        static_cast<void>(bytes);
        return true;
    }

    /// Submits one complete controller response and reports queue acceptance.
    virtual bool send(core::Frame frame) = 0;

    /// Publishes one normative transfer diagnostic without binding logging APIs.
    virtual void diagnose(ControllerTransferDiagnostic diagnostic) = 0;

    /// Publishes controller-update state without coupling to its representation.
    virtual void publish(FirmwareTransferEvent event, std::uint32_t index,
                         std::uint32_t frame_count) = 0;
};

/// Processes the `0xC1` through `0xC5` firmware-transfer exchange.
class ControllerFirmwareTransfer {
public:
    /// Processes one accepted family frame and then applies its conditional timeout.
    void handle(const core::Frame& frame, std::uint64_t now_milliseconds,
                ControllerFirmwarePort& port);

    /// Reports whether ordinary host-to-controller traffic must be suppressed.
    bool active() const;

    /// Exposes retained geometry for status and deterministic tests.
    std::uint32_t frame_count() const;

    /// Exposes the retained block size for status and deterministic tests.
    std::uint16_t frame_data_size() const;

private:
    void handle_start(std::uint64_t now_milliseconds, ControllerFirmwarePort& port);
    void handle_geometry(core::BytesView payload, std::uint64_t now_milliseconds,
                         ControllerFirmwarePort& port);
    void handle_data(core::BytesView payload, ControllerFirmwarePort& port);
    void finish(FirmwareTransferEvent event, ControllerFirmwarePort& port);
    void report_error(ControllerFirmwarePort& port);
    void apply_timeout(std::uint64_t now_milliseconds, ControllerFirmwarePort& port);

    bool active_ = false;
    bool waiting_ = false;
    std::uint64_t wait_started_milliseconds_ = 0U;
    std::uint32_t frame_count_ = 0U;
    std::uint16_t frame_data_size_ = 0U;
};

}  // namespace firmware::application
