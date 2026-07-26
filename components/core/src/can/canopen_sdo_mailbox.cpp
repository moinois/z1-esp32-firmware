// Implements serialized SDO request admission, matching, and deadlines.
#include "firmware/core/canopen_sdo_mailbox.hpp"

namespace firmware::core {

std::optional<CanFrame> CanopenSdoMailbox::begin_upload(
    std::uint8_t node, std::uint16_t index, std::uint8_t subindex,
    std::uint64_t now_milliseconds, std::uint32_t timeout_milliseconds) {
    if (pending_.has_value()) return std::nullopt;
    pending_ = Pending{node, index, subindex,
                       now_milliseconds + timeout_milliseconds};
    return make_sdo_upload_request(node, index, subindex);
}

std::optional<CanFrame> CanopenSdoMailbox::begin_download(
    std::uint8_t node, std::uint16_t index, std::uint8_t subindex,
    std::uint32_t value, std::uint64_t now_milliseconds,
    std::uint32_t timeout_milliseconds) {
    if (pending_.has_value()) return std::nullopt;
    pending_ = Pending{node, index, subindex,
                       now_milliseconds + timeout_milliseconds};
    return make_sdo_download_request(node, index, subindex, value);
}

std::optional<SdoClientResponse> CanopenSdoMailbox::accept(
    const CanFrame& frame) {
    if (!pending_.has_value()) return std::nullopt;
    const Pending request = *pending_;
    const auto response = parse_sdo_client_response(
        frame, request.node, request.index, request.subindex);
    if (!response.has_value()) return std::nullopt;
    pending_.reset();
    return response;
}

bool CanopenSdoMailbox::timed_out(std::uint64_t now_milliseconds) {
    if (!pending_.has_value() || now_milliseconds < pending_->deadline) {
        return false;
    }
    pending_.reset();
    return true;
}

bool CanopenSdoMailbox::pending() const {
    return pending_.has_value();
}

}  // namespace firmware::core
