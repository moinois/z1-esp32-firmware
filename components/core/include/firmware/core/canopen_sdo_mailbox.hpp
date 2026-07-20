// Declares one-outstanding-operation SDO client mailbox state.
#pragma once

#include "firmware/core/canopen_sdo_client.hpp"

#include <cstdint>
#include <optional>

namespace firmware::core {

// Owns one serialized SDO request and matches its response or timeout.
class CanopenSdoMailbox {
public:
    // Starts an expedited upload when no operation is currently pending.
    std::optional<CanFrame> begin_upload(std::uint8_t node, std::uint16_t index,
                                         std::uint8_t subindex,
                                         std::uint64_t now_milliseconds,
                                         std::uint32_t timeout_milliseconds);

    // Starts an expedited download when no operation is currently pending.
    std::optional<CanFrame> begin_download(std::uint8_t node, std::uint16_t index,
                                           std::uint8_t subindex,
                                           std::uint32_t value,
                                           std::uint64_t now_milliseconds,
                                           std::uint32_t timeout_milliseconds);

    // Accepts a matching response and clears the pending operation.
    std::optional<SdoClientResponse> accept(const CanFrame& frame);

    // Reports and clears a request whose absolute deadline has elapsed.
    bool timed_out(std::uint64_t now_milliseconds);

    // Reports whether a request is currently awaiting a response.
    bool pending() const;

private:
    struct Pending {
        std::uint8_t node;
        std::uint16_t index;
        std::uint8_t subindex;
        std::uint64_t deadline;
    };

    std::optional<Pending> pending_;
};

}  // namespace firmware::core
