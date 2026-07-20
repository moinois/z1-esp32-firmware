// Defines the controller factory-data transfer service and replaceable I/O port.
#pragma once

#include "firmware/core/frame.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace firmware::application {

// Isolates factory-transfer rules from storage and controller output.
class ControllerFactoryPort {
public:
    // Enables safe destruction through a substituted port implementation.
    virtual ~ControllerFactoryPort() = default;

    // Reports whether the factory-data file is currently available.
    virtual bool file_exists(std::string_view path) = 0;

    // Reads the file as fixed-size input chunks or reports failure.
    virtual std::optional<std::vector<core::ByteVector>> read_chunks(
        std::string_view path, std::size_t chunk_size) = 0;

    // Attempts one removal of the requested file.
    virtual bool remove_file(std::string_view path) = 0;

    // Submits one complete response and reports queue acceptance.
    virtual bool send(core::Frame frame) = 0;
};

// Processes the `0xE1` through `0xE5` factory-data exchange.
class ControllerFactoryTransfer {
public:
    // Processes one accepted factory-family frame.
    void handle(const core::Frame& frame, ControllerFactoryPort& port);

    // Reports whether ordinary host-to-controller traffic must be suppressed.
    bool active() const;

    // Exposes the retained eligible-record count.
    std::uint32_t frame_count() const;

    // Exposes the retained controller-selected record size.
    std::uint16_t frame_data_size() const;

private:
    void handle_start(ControllerFactoryPort& port);
    void handle_geometry(core::BytesView payload, ControllerFactoryPort& port);
    void handle_data(core::BytesView payload, ControllerFactoryPort& port);
    void finish(bool remove_file, ControllerFactoryPort& port);
    void report_error(ControllerFactoryPort& port);

    bool active_ = false;
    std::uint32_t frame_count_ = 0U;
    std::uint16_t frame_data_size_ = 0U;
};

}  // namespace firmware::application
