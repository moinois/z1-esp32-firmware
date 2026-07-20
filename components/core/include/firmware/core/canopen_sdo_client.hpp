// Declares deterministic expedited SDO client request and response codecs.
#pragma once

#include "firmware/core/canopen_node.hpp"

#include <cstdint>
#include <optional>

namespace firmware::core {

// Describes one decoded expedited upload response or protocol abort.
struct SdoClientResponse {
    std::uint16_t index = 0U;
    std::uint8_t subindex = 0U;
    std::uint32_t value = 0U;
    bool aborted = false;
};

// Creates one 32-bit expedited upload request for a remote node.
CanFrame make_sdo_upload_request(std::uint8_t node, std::uint16_t index,
                                 std::uint8_t subindex);

// Creates one 32-bit expedited download request for a remote node.
CanFrame make_sdo_download_request(std::uint8_t node, std::uint16_t index,
                                   std::uint8_t subindex, std::uint32_t value);

// Parses a matching expedited response, rejecting wrong node/object frames.
std::optional<SdoClientResponse> parse_sdo_client_response(
    const CanFrame& frame, std::uint8_t node, std::uint16_t index,
    std::uint8_t subindex);

}  // namespace firmware::core
