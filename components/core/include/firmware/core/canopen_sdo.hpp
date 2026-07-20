// Declares an expedited CANopen SDO server over the portable object dictionary.
#pragma once

#include "firmware/core/canopen_dictionary.hpp"
#include "firmware/core/canopen_node.hpp"

#include <optional>

namespace firmware::core {

// Couples one SDO response with target-independent effects from a successful write.
struct SdoServerResult {
    CanFrame frame;
    DictionaryWriteEffects effects;
};

// Handles local expedited scalar uploads and downloads without block state.
class CanopenSdoServer {
public:
    // Binds the wire server to the dictionary whose values it exposes.
    explicit CanopenSdoServer(CanopenObjectDictionary& dictionary);

    // Returns one response for a complete local SDO request, or ignores other frames.
    std::optional<SdoServerResult> handle(const CanFrame& request);

private:
    // Creates a standard abort response retaining the requested address.
    static SdoServerResult abort_response(const CanFrame& request,
                                           SdoAbort abort);

    // Creates a response with the local identifier and copied object address.
    static CanFrame response_header(const CanFrame& request,
                                    std::uint8_t command);

    CanopenObjectDictionary& dictionary_;
};

}  // namespace firmware::core
