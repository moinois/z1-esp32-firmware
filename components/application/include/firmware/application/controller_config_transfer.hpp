/** @file @brief Defines the controller configuration-transfer service and replaceable I/O port. */
#pragma once

#include "firmware/core/frame.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace firmware::application {

/// Isolates configuration-transfer rules from storage and controller output.
class ControllerConfigPort {
public:
    /// Enables safe destruction through a substituted port implementation.
    virtual ~ControllerConfigPort() = default;

    /// Reports whether the configuration file is currently available.
    virtual bool configuration_available() = 0;

    /// Reads the file as fixed-size input chunks or reports failure.
    virtual std::optional<std::vector<core::ByteVector>>
    read_configuration_chunks(std::size_t chunk_size) = 0;

    /// Submits one complete response and reports queue acceptance.
    virtual bool send(core::Frame frame) = 0;
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
