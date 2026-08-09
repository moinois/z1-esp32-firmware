/** @file @brief Implements BLUFI GATT response ordering, aggregation, and notification retry. */
#include "application/provisioning/blufi_gatt.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t prepared_capacity = 1024U;
constexpr std::uint32_t notification_retry_milliseconds = 10U;
constexpr std::uint8_t fixed_read_value = 0U;

}  // namespace

BlufiGattPolicy::BlufiGattPolicy(BlufiGattPort& port) : port_(port) {}

void BlufiGattPolicy::reset() {
    discard_prepared();
}

core::ByteVector BlufiGattPolicy::read_outgoing_characteristic() const {
    return {fixed_read_value};
}

core::ByteVector BlufiGattPolicy::read_client_configuration() const {
    return {fixed_read_value};
}

void BlufiGattPolicy::write_control(core::BytesView value,
                                    bool response_requested) {
    if (response_requested) {
        port_.send_write_response(BlufiAttStatus::success);
    }
    port_.decode_frame(value);
}

void BlufiGattPolicy::write_client_configuration(core::BytesView value,
                                                 bool response_requested) {
    static_cast<void>(value);
    if (response_requested) {
        port_.send_write_response(BlufiAttStatus::success);
    }
}

BlufiPreparedWriteResponse BlufiGattPolicy::prepare_write(
    std::uint16_t handle, std::size_t offset, core::BytesView value) {
    if (offset > prepared_capacity) {
        return reject_prepared(BlufiAttStatus::invalid_offset, handle, offset);
    }
    if (value.size() > prepared_capacity - offset) {
        return reject_prepared(BlufiAttStatus::invalid_length, handle, offset);
    }
    if (!prepared_.has_value() && offset != 0U) {
        return reject_prepared(BlufiAttStatus::invalid_offset, handle, offset);
    }
    if (!prepared_.has_value()) {
        prepared_ = port_.allocate_prepared(prepared_capacity);
        if (!prepared_.has_value() || prepared_->size() != prepared_capacity) {
            return reject_prepared(BlufiAttStatus::allocation_failure, handle,
                                   offset);
        }
    }

    std::copy(value.begin(), value.end(), prepared_->begin() + offset);
    recorded_length_ += value.size();
    return {BlufiAttStatus::success,
            handle,
            offset,
            core::ByteVector(value.begin(), value.end())};
}

BlufiAttStatus BlufiGattPolicy::finish_prepared(bool execute) {
    if (execute && prepared_.has_value()) {
        // Overlaps and gaps are unspecified by the product. Clamping prevents
        // the source firmware's possible out-of-range read in this safe port.
        const std::size_t safe_length =
            std::min(recorded_length_, prepared_->size());
        port_.decode_frame(core::BytesView(prepared_->data(), safe_length));
    }
    discard_prepared();
    return BlufiAttStatus::success;
}

bool BlufiGattPolicy::notify(core::BytesView frame) {
    while (port_.connected()) {
        if (port_.send_notification(frame)) {
            return true;
        }
        if (!port_.connected()) {
            return false;
        }
        port_.delay_milliseconds(notification_retry_milliseconds);
    }
    return false;
}

void BlufiGattPolicy::discard_prepared() {
    prepared_.reset();
    recorded_length_ = 0U;
}

BlufiPreparedWriteResponse BlufiGattPolicy::reject_prepared(
    BlufiAttStatus status, std::uint16_t handle, std::size_t offset) {
    discard_prepared();
    return {status, handle, offset, {}};
}

}  // namespace firmware::application
