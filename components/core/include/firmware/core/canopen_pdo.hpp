/** @file @brief Transport-neutral RPDO admission for the local CANopen node. */
#pragma once

#include "firmware/core/canopen_node.hpp"
#include "firmware/core/canopen_dictionary.hpp"

namespace firmware::core {

/** Applies enabled receive-PDO mappings to the local object dictionary. */
class CanopenReceivePdoRouter {
public:
    /// Binds routing to the mutable dictionary that owns PDO configuration.
    explicit CanopenReceivePdoRouter(CanopenObjectDictionary& dictionary);

    /** Accepts one matching RPDO and reports whether it was consumed.
     *  Disabled, malformed, or unrelated frames leave the dictionary untouched.
     */
    bool receive(const CanFrame& frame);

private:
    /// Attempts one configured RPDO in deterministic object order.
    bool receive_from(std::uint8_t pdo_number, const CanFrame& frame);

    CanopenObjectDictionary& dictionary_;
};

}  // namespace firmware::core
