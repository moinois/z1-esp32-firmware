// Declares deterministic event-timed TPDO generation from dictionary mappings.
#pragma once

#include "firmware/core/canopen_dictionary.hpp"
#include "firmware/core/canopen_node.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace firmware::core {

// Schedules one enabled TPDO at a time without depending on a CAN driver.
class CanopenTransmitPdoScheduler {
public:
    // Binds event generation to the mutable object dictionary.
    explicit CanopenTransmitPdoScheduler(
        const CanopenObjectDictionary& dictionary);

    // Advances one 10 ms cycle and returns an event-due TPDO, if any.
    std::optional<CanFrame> process_cycle();

private:
    // Builds one configured TPDO when its timer has elapsed.
    std::optional<CanFrame> build_tpdo(std::uint8_t pdo_number);

    const CanopenObjectDictionary& dictionary_;
    std::array<std::uint32_t, 4U> elapsed_milliseconds_{};
};

}  // namespace firmware::core
