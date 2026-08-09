/** @file @brief Composes CANopen state machines and delegates only physical effects outward. */
#include "application/can/canopen_service.hpp"

#include <cstdint>

namespace firmware::application {

CanopenService::CanopenService(CanopenServicePort& port)
    : port_(port),
      receive_pdo_router_(dictionary_),
      transmit_pdo_scheduler_(dictionary_),
      sdo_server_(dictionary_) {}

void CanopenService::receive(const core::CanFrame& frame) {
    const core::NmtRequestEffect nmt_effect = node_.accept_nmt(frame);
    if (nmt_effect == core::NmtRequestEffect::communication_reset) {
        dictionary_.communication_reset();
    }

    if (receive_pdo_router_.receive(frame)) {
        return;
    }

    const auto response = sdo_server_.handle(frame);
    if (!response.has_value()) {
        return;
    }
    apply_write_effects(response->effects);
    port_.transmit(response->frame);
}

void CanopenService::process_cycle() {
    const core::CanopenCycleResult result = node_.process_cycle();
    if (result.frame.has_value()) {
        port_.transmit(*result.frame);
    }
    if (const auto pdo = transmit_pdo_scheduler_.process_cycle(); pdo.has_value()) {
        port_.transmit(*pdo);
    }
    if (result.restart_mainboard) {
        port_.restart_mainboard();
    }
}

void CanopenService::set_error_register(std::uint8_t value) {
    dictionary_.set_error_register(value);
    node_.set_error_register(value);
}

const core::CanopenObjectDictionary& CanopenService::dictionary() const {
    return dictionary_;
}

const core::CanopenNode& CanopenService::node() const {
    return node_;
}

void CanopenService::apply_write_effects(
    const core::DictionaryWriteEffects& effects) {
    if (effects.producer_heartbeat_period.has_value()) {
        node_.set_producer_heartbeat_period(
            *effects.producer_heartbeat_period);
    }
}

}  // namespace firmware::application
