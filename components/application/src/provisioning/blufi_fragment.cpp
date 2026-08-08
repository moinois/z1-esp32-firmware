/** @file @brief Implements BLUFI MTU sizing, remaining prefixes, and retained reassembly. */
#include "firmware/application/blufi_fragment.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::uint16_t default_att_mtu = 23U;
constexpr std::uint16_t maximum_blufi_mtu = 255U;
constexpr std::size_t fragment_protocol_overhead = 11U;
constexpr std::size_t remaining_length_size = 2U;
constexpr std::uint8_t allocation_error = 5U;
constexpr std::uint8_t data_format_error = 9U;
constexpr std::uint8_t fragment_state_error = 12U;

}  // namespace

BlufiFragmentSession::BlufiFragmentSession(BlufiFragmentPort& port)
    : port_(port), att_mtu_(default_att_mtu) {}

void BlufiFragmentSession::reset() {
    clear_partial();
}

void BlufiFragmentSession::set_att_mtu(std::uint16_t mtu) {
    att_mtu_ = mtu;
}

bool BlufiFragmentSession::send_data(std::uint8_t subtype,
                                     core::BytesView message) {
    constexpr std::size_t maximum_logical_size =
        std::numeric_limits<std::uint16_t>::max();
    const std::size_t content_capacity = fragment_content_capacity();
    if (message.size() > maximum_logical_size || content_capacity == 0U) {
        port_.report_error(data_format_error);
        return false;
    }
    if (message.size() <= content_capacity) {
        return port_.send_data(subtype, message, false);
    }

    std::size_t offset = 0U;
    while (message.size() - offset > content_capacity) {
        const std::size_t remaining = message.size() - offset;
        core::ByteVector fragment;
        fragment.reserve(remaining_length_size + content_capacity);
        fragment.push_back(static_cast<std::uint8_t>(remaining & 0xFFU));
        fragment.push_back(static_cast<std::uint8_t>(remaining >> 8U));
        fragment.insert(fragment.end(), message.begin() + offset,
                        message.begin() + offset + content_capacity);
        if (!port_.send_data(subtype, fragment, true)) {
            return false;
        }
        offset += content_capacity;
    }
    const core::BytesView final_fragment(message.data() + offset,
                                         message.size() - offset);
    return port_.send_data(subtype, final_fragment, false);
}

std::optional<BlufiIncomingFrame> BlufiFragmentSession::receive(
    BlufiIncomingFrame frame) {
    if (!frame.non_final_fragment && !fragmented_input_active_) {
        return frame;
    }

    if (frame.non_final_fragment) {
        if (frame.data.size() < remaining_length_size) {
            // The source firmware accesses this malformed prefix out of range.
            // Reporting a format error keeps this C++ implementation defined.
            port_.report_error(data_format_error);
            return std::nullopt;
        }
        if (partial_message_.has_value() && accumulated_size_ == 0U) {
            port_.report_error(fragment_state_error);
            return std::nullopt;
        }

        if (!partial_message_.has_value()) {
            const std::size_t required_size =
                static_cast<std::size_t>(frame.data[0]) |
                (static_cast<std::size_t>(frame.data[1]) << 8U);
            fragmented_input_active_ = true;
            partial_type_ = frame.type;
            partial_subtype_ = frame.subtype;
            partial_message_ = port_.allocate_message(required_size);
            if (!partial_message_.has_value() ||
                partial_message_->size() != required_size) {
                partial_message_.reset();
                port_.report_error(allocation_error);
                return std::nullopt;
            }
        } else if (!matches_partial(frame)) {
            port_.report_error(data_format_error);
            return std::nullopt;
        }

        const std::size_t content_size =
            frame.data.size() - remaining_length_size;
        if (content_size > partial_message_->size() - accumulated_size_) {
            port_.report_error(data_format_error);
            return std::nullopt;
        }
        std::copy(frame.data.begin() + remaining_length_size, frame.data.end(),
                  partial_message_->begin() + accumulated_size_);
        accumulated_size_ += content_size;
        return std::nullopt;
    }

    if (!partial_message_.has_value()) {
        port_.report_error(allocation_error);
        return std::nullopt;
    }
    if (!matches_partial(frame) ||
        frame.data.size() != partial_message_->size() - accumulated_size_) {
        port_.report_error(data_format_error);
        return std::nullopt;
    }

    std::copy(frame.data.begin(), frame.data.end(),
              partial_message_->begin() + accumulated_size_);
    BlufiIncomingFrame complete{
        partial_type_, partial_subtype_, std::move(*partial_message_), false};
    clear_partial();
    return complete;
}

std::size_t BlufiFragmentSession::fragment_content_capacity() const {
    const std::size_t effective_mtu =
        std::min<std::uint16_t>(att_mtu_, maximum_blufi_mtu);
    if (effective_mtu <= fragment_protocol_overhead) {
        return 0U;
    }
    return effective_mtu - fragment_protocol_overhead;
}

bool BlufiFragmentSession::matches_partial(
    const BlufiIncomingFrame& frame) const {
    return frame.type == partial_type_ && frame.subtype == partial_subtype_;
}

void BlufiFragmentSession::clear_partial() {
    partial_message_.reset();
    accumulated_size_ = 0U;
    partial_type_ = BlufiFrameType::control;
    partial_subtype_ = 0U;
    fragmented_input_active_ = false;
}

}  // namespace firmware::application
