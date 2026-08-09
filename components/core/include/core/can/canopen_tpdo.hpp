/** @file @brief Deterministic event-timed TPDO generation from mappings. */
#pragma once

#include "core/can/canopen_dictionary.hpp"
#include "core/can/canopen_node.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace firmware::core {

/** Schedules enabled TPDOs without depending on a physical CAN driver. */
class CanopenTransmitPdoScheduler {
public:
    /// Binds event generation to the dictionary containing mapping and timers.
    explicit CanopenTransmitPdoScheduler(
        const CanopenObjectDictionary& dictionary);

    /** Advances one CANopen cycle and returns at most one event-due TPDO. */
    std::optional<CanFrame> process_cycle();

private:
    /// Builds one configured TPDO after validating its current mapping.
    std::optional<CanFrame> build_tpdo(std::uint8_t pdo_number);

    const CanopenObjectDictionary& dictionary_;
    std::array<std::uint32_t, canopen_dictionary::pdo_count>
        elapsed_milliseconds_{};
};

}  // namespace firmware::core
