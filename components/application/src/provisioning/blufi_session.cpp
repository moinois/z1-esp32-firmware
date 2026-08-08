/** @file @brief Composes the portable BLUFI wire, fragment, security, and product stages. */
#include "firmware/application/blufi_session.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::uint8_t error_data_subtype = 0x12U;

}  // namespace

BlufiConnectionSession::BlufiConnectionSession(
    BlufiCipher& cipher, BlufiNegotiationHandler& negotiation,
    BlufiProductActions& actions, BlufiSessionTransport& transport)
    : cipher_(cipher),
      negotiation_(negotiation),
      actions_(actions),
      transport_(transport),
      wire_(cipher, *this),
      fragments_(*this),
      products_(*this) {}

void BlufiConnectionSession::reset() {
    wire_.reset();
    fragments_.reset();
    security_ready_notified_ = false;
}

void BlufiConnectionSession::set_att_mtu(std::uint16_t mtu) {
    fragments_.set_att_mtu(mtu);
}

void BlufiConnectionSession::receive_characteristic(core::BytesView frame) {
    auto wire_frame = wire_.receive(frame);
    if (!wire_frame.has_value()) {
        return;
    }
    auto product_message = fragments_.receive(std::move(*wire_frame));
    if (product_message.has_value()) {
        products_.dispatch(*product_message);
    }
}

bool BlufiConnectionSession::send_product_data(std::uint8_t subtype,
                                               core::BytesView data) {
    return fragments_.send_data(subtype, data);
}

void BlufiConnectionSession::report_protocol_error(std::uint8_t error) {
    report_error(error);
}

void BlufiConnectionSession::send_characteristic(core::BytesView frame) {
    transport_.send_characteristic(frame);
}

bool BlufiConnectionSession::send_data(std::uint8_t subtype,
                                       core::BytesView data,
                                       bool non_final) {
    return wire_.send(BlufiFrameType::data, subtype, data, false, non_final);
}

std::optional<core::ByteVector> BlufiConnectionSession::allocate_message(
    std::size_t size) {
    return transport_.allocate_message(size);
}

void BlufiConnectionSession::report_error(std::uint8_t error) {
    const core::ByteVector payload{error};
    static_cast<void>(fragments_.send_data(error_data_subtype, payload));
}

void BlufiConnectionSession::receive_negotiation(core::BytesView data) {
    negotiation_.receive_negotiation(data);
    if (cipher_.ready() && !security_ready_notified_) {
        security_ready_notified_ = true;
        actions_.security_negotiated();
    }
}

void BlufiConnectionSession::connect_station() {
    actions_.connect_station();
}

void BlufiConnectionSession::disconnect_station() {
    actions_.disconnect_station();
}

void BlufiConnectionSession::request_wifi_status() {
    actions_.request_wifi_status();
}

void BlufiConnectionSession::request_wifi_list() {
    actions_.request_wifi_list();
}

void BlufiConnectionSession::receive_ssid(core::BytesView data) {
    actions_.receive_ssid(data);
}

void BlufiConnectionSession::receive_password(core::BytesView data) {
    actions_.receive_password(data);
}

void BlufiConnectionSession::receive_error(std::uint8_t error) {
    actions_.receive_error(error);
}

void BlufiConnectionSession::receive_custom_data(core::BytesView data) {
    actions_.receive_custom_data(data);
}

void BlufiConnectionSession::send_data(std::uint8_t subtype,
                                       core::BytesView data) {
    static_cast<void>(send_product_data(subtype, data));
}

}  // namespace firmware::application
