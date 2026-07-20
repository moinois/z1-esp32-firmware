// Declares composition of the portable CANopen node, dictionary, and SDO server.
#pragma once

#include "firmware/core/canopen_dictionary.hpp"
#include "firmware/core/canopen_node.hpp"
#include "firmware/core/canopen_sdo.hpp"

#include <cstdint>

namespace firmware::application {

// Isolates CANopen composition from bus transmission, diagnostics, and reset.
class CanopenServicePort {
public:
    // Enables safe destruction through a substituted service adapter.
    virtual ~CanopenServicePort() = default;

    // Queues one CANopen output frame for the target bus.
    virtual void transmit(const core::CanFrame& frame) = 0;

    // Reports a digital output dictionary write to diagnostics.
    virtual void report_digital_output(std::uint32_t value) = 0;

    // Requests the delayed mainboard reset selected by NMT.
    virtual void restart_mainboard() = 0;
};

// Keeps NMT, dictionary, SDO, and their external effects mutually consistent.
class CanopenService {
public:
    // Creates one fresh local node bound to replaceable external operations.
    explicit CanopenService(CanopenServicePort& port);

    // Routes one received CAN frame through NMT and the local SDO server.
    void receive(const core::CanFrame& frame);

    // Advances one required 10 ms node cycle and publishes its effects.
    void process_cycle();

    // Applies one error-register sample to both dictionary and NMT policy.
    void set_error_register(std::uint8_t value);

    // Exposes read-only dictionary state for PDO and diagnostic adapters.
    const core::CanopenObjectDictionary& dictionary() const;

    // Exposes read-only node state for service and diagnostic adapters.
    const core::CanopenNode& node() const;

private:
    // Publishes all target-independent effects from a successful SDO write.
    void apply_write_effects(const core::DictionaryWriteEffects& effects);

    CanopenServicePort& port_;
    core::CanopenObjectDictionary dictionary_;
    core::CanopenNode node_;
    core::CanopenSdoServer sdo_server_;
};

}  // namespace firmware::application
