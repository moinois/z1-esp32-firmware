/** @file @brief Declares composition of the portable CANopen node, dictionary, and SDO server. */
#pragma once

#include "core/can/canopen_dictionary.hpp"
#include "core/can/canopen_node.hpp"
#include "core/can/canopen_sdo.hpp"
#include "core/can/canopen_pdo.hpp"
#include "core/can/canopen_tpdo.hpp"

#include <cstdint>

namespace firmware::application {

/// Isolates CANopen composition from bus transmission, diagnostics, and reset.
class CanopenServicePort {
public:
    /// Enables safe destruction through a substituted service adapter.
    virtual ~CanopenServicePort() = default;

    /// Queues one CANopen output frame for the target bus.
    virtual void transmit(const core::CanFrame& frame) = 0;

    /// Requests the delayed mainboard reset selected by NMT.
    virtual void restart_mainboard() = 0;
};

/// Keeps NMT, dictionary, SDO, and their external effects mutually consistent.
class CanopenService {
public:
    /// Creates one fresh local node bound to replaceable external operations.
    explicit CanopenService(CanopenServicePort& port);

    /// Routes one received CAN frame through NMT and the local SDO server.
    void receive(const core::CanFrame& frame);

    /// Advances one required 10 ms node cycle and publishes its effects.
    void process_cycle();

    /// Applies one error-register sample to both dictionary and NMT policy.
    void set_error_register(std::uint8_t value);

    /// Exposes read-only dictionary state for PDO and diagnostic adapters.
    const core::CanopenObjectDictionary& dictionary() const;

    /// Exposes read-only node state for service and diagnostic adapters.
    const core::CanopenNode& node() const;

private:
    /// Applies node state effects from a successful SDO write.
    void apply_write_effects(const core::DictionaryWriteEffects& effects);

    CanopenServicePort& port_;
    core::CanopenObjectDictionary dictionary_;
    core::CanopenReceivePdoRouter receive_pdo_router_;
    core::CanopenTransmitPdoScheduler transmit_pdo_scheduler_;
    core::CanopenNode node_;
    core::CanopenSdoServer sdo_server_;
};

}  // namespace firmware::application
