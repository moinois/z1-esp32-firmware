/** @file @brief Implements factory-data record selection and lifecycle behavior. */
#include "application/controller/controller_factory_transfer.hpp"

#include "application/controller/controller_transfer.hpp"
#include "core/protocol/protocol_constants.hpp"
#include "core/filesystem/sd_user_path.hpp"

#include <utility>

namespace firmware::application {
namespace {

const std::string factory_path = core::physical_sd_path("/factory.ini");
constexpr std::size_t maximum_record_size = 132U;

// Reports whether a raw factory chunk contributes to geometry and selection.
bool eligible_record(const core::ByteVector& chunk) {
    return chunk.size() > 2U && chunk.front() != '#';
}

}  // namespace

void ControllerFactoryTransfer::handle(const core::Frame& frame, ControllerFactoryPort& port) {
    switch (transfer_operation(frame.type)) {
        case TransferOperation::start:
            handle_start(port);
            break;
        case TransferOperation::geometry:
            handle_geometry(frame.payload, port);
            break;
        case TransferOperation::data:
            handle_data(frame.payload, port);
            break;
        case TransferOperation::complete:
            finish(true, port);
            break;
        case TransferOperation::cancel:
            finish(false, port);
            break;
        case TransferOperation::unknown:
            break;
    }
}

void ControllerFactoryTransfer::handle_start(ControllerFactoryPort& port) {
    const bool acknowledgement_sent = port.send(make_transfer_reply(
        core::protocol::factory_family, core::protocol::transfer_start));
    const bool available = port.file_exists(factory_path);
    if (available) {
        port.diagnose(controller_transfer_diagnostic(
            ControllerTransferFamily::factory,
            ControllerTransferDiagnosticEvent::start));
        active_ = true;
    }
    if (!acknowledgement_sent || !available) {
        if (!available) port.diagnose(controller_transfer_diagnostic(
            ControllerTransferFamily::factory,
            ControllerTransferDiagnosticEvent::missing_content));
        report_error(port);
    }
}

void ControllerFactoryTransfer::handle_geometry(core::BytesView payload,
                                                ControllerFactoryPort& port) {
    const auto proposed = parse_transfer_geometry(payload);
    if (!proposed.has_value() || proposed->frame_data_size == 0U ||
        proposed->frame_data_size > controller_transfer_frame_data_size) {
        if (payload.size() < 6U) port.diagnose(controller_transfer_diagnostic(
            ControllerTransferFamily::factory,
            ControllerTransferDiagnosticEvent::short_layout));
        port.diagnose(controller_transfer_diagnostic(
            ControllerTransferFamily::factory,
            ControllerTransferDiagnosticEvent::layout));
        report_error(port);
        return;
    }
    const auto chunks =
        port.read_chunks(factory_path, controller_transfer_chunk_size);
    if (!chunks.has_value()) {
        port.diagnose(controller_transfer_diagnostic(
            ControllerTransferFamily::factory,
            ControllerTransferDiagnosticEvent::data_open_failure));
        port.diagnose(controller_transfer_diagnostic(
            ControllerTransferFamily::factory,
            ControllerTransferDiagnosticEvent::layout));
        report_error(port);
        return;
    }

    std::uint32_t count = 0U;
    for (const auto& chunk : *chunks) {
        if (eligible_record(chunk)) {
            ++count;
        }
    }
    frame_count_ = count;
    frame_data_size_ = proposed->frame_data_size;
    if (!port.send(make_transfer_reply(core::protocol::factory_family,
                                       core::protocol::transfer_geometry,
                                       encode_transfer_geometry(frame_count_,
                                                                frame_data_size_)))) {
        port.diagnose(controller_transfer_diagnostic(
            ControllerTransferFamily::factory,
            ControllerTransferDiagnosticEvent::layout_reply_failure));
        report_error(port);
    }
    port.diagnose(controller_transfer_diagnostic(
        ControllerTransferFamily::factory,
        ControllerTransferDiagnosticEvent::layout));
}

void ControllerFactoryTransfer::handle_data(core::BytesView payload,
                                            ControllerFactoryPort& port) {
    port.diagnose(controller_transfer_diagnostic(
        ControllerTransferFamily::factory,
        ControllerTransferDiagnosticEvent::data));
    const auto request = parse_transfer_data_request(payload);
    if (!request.has_value()) {
        port.diagnose(controller_transfer_diagnostic(
            ControllerTransferFamily::factory,
            ControllerTransferDiagnosticEvent::short_data));
        report_error(port);
        return;
    }
    port.diagnose(controller_transfer_diagnostic(
        ControllerTransferFamily::factory,
        ControllerTransferDiagnosticEvent::data_request, request->index));
    if (request->index == 0U || frame_data_size_ == 0U) {
        return;
    }
    const auto chunks =
        port.read_chunks(factory_path, controller_transfer_chunk_size);
    if (!chunks.has_value()) {
        port.diagnose(controller_transfer_diagnostic(
            ControllerTransferFamily::factory,
            ControllerTransferDiagnosticEvent::data_open_failure));
        report_error(port);
        return;
    }

    std::uint32_t eligible_index = 0U;
    for (const auto& chunk : *chunks) {
        if (!eligible_record(chunk)) {
            continue;
        }
        ++eligible_index;
        if (eligible_index != request->index) {
            continue;
        }
        port.diagnose(controller_transfer_diagnostic(
            ControllerTransferFamily::factory,
            ControllerTransferDiagnosticEvent::data_sent, request->index));
        if (chunk.size() > maximum_record_size || chunk.size() > frame_data_size_) {
            return;
        }
        const std::size_t response_size = request->wire_index.size() + chunk.size();
        if (!port.response_data_memory_available(response_size)) {
            port.diagnose(controller_transfer_diagnostic(
                ControllerTransferFamily::factory,
                ControllerTransferDiagnosticEvent::frame_data_allocation_failure));
            report_error(port);
            return;
        }
        core::ByteVector response = request->wire_index;
        response.insert(response.end(), chunk.begin(), chunk.end());
        if (!port.send(make_transfer_reply(core::protocol::factory_family,
                                           core::protocol::transfer_data,
                                           std::move(response)))) {
            report_error(port);
        }
        return;
    }
}

void ControllerFactoryTransfer::finish(bool remove_file, ControllerFactoryPort& port) {
    active_ = false;
    frame_count_ = 0U;
    frame_data_size_ = 0U;
    if (remove_file) {
        static_cast<void>(port.remove_file(factory_path));
    }
}

void ControllerFactoryTransfer::report_error(ControllerFactoryPort& port) {
    port.send(make_transfer_reply(core::protocol::factory_family,
                                  core::protocol::transfer_cancel));
}

bool ControllerFactoryTransfer::active() const {
    return active_;
}

std::uint32_t ControllerFactoryTransfer::frame_count() const {
    return frame_count_;
}

std::uint16_t ControllerFactoryTransfer::frame_data_size() const {
    return frame_data_size_;
}

}  // namespace firmware::application
