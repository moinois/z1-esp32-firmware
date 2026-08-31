/** @file @brief Defines the controller configuration-transfer service and replaceable I/O port. */
#pragma once

#include "core/protocol/frame.hpp"
#include "application/diagnostics/controller_diagnostics.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace firmware::application {

/// One bounded configuration-file line read and whether that read observed EOF.
struct ControllerConfigurationRead {
    core::ByteVector observed_text;
    bool observed_end_of_file = false;
};

/// Isolates configuration-transfer rules from storage and controller output.
class ControllerConfigPort {
public:
    /// Enables safe destruction through a substituted port implementation.
    virtual ~ControllerConfigPort() = default;

    /// Reports whether the configuration file is currently available.
    virtual bool configuration_available() = 0;

    /// Performs sequential bounded line reads or reports file-access failure.
    virtual std::optional<std::vector<ControllerConfigurationRead>>
    read_configuration_lines(std::size_t maximum_read_size) = 0;

    /// Probes transient data-response retention for deterministic DIAG-035 handling.
    virtual bool response_data_memory_available(std::size_t bytes) {
        static_cast<void>(bytes);
        return true;
    }

    /// Submits one complete response and reports queue acceptance.
    virtual bool send(core::Frame frame) = 0;

    /// Publishes one normative transfer diagnostic without binding logging APIs.
    virtual void diagnose(ControllerTransferDiagnostic diagnostic) = 0;
};

/// Processes the `0xD1` through `0xD5` configuration exchange.
class ControllerConfigTransfer {
public:
    /// Processes one accepted configuration-family frame.
    void handle(const core::Frame& frame, ControllerConfigPort& port);

    /// Reports whether ordinary host-to-controller traffic must be suppressed.
    bool active() const;

    /// Exposes the retained count of transferable input chunks.
    std::uint32_t frame_count() const;

    /// Exposes the fixed retained response data size.
    std::uint16_t frame_data_size() const;

private:
    void handle_start(ControllerConfigPort& port);
    void handle_geometry(core::BytesView payload, ControllerConfigPort& port);
    void handle_data(core::BytesView payload, ControllerConfigPort& port);
    void finish();
    void report_error(ControllerConfigPort& port);

    bool active_ = false;
    std::uint32_t frame_count_ = 0U;
    std::uint16_t frame_data_size_ = 0U;
};

}  // namespace firmware::application
