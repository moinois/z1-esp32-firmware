/** @file @brief Expedited CANopen SDO server over the portable dictionary. */
#pragma once

#include "core/can/canopen_dictionary.hpp"
#include "core/can/canopen_node.hpp"

#include <optional>

namespace firmware::core {

/** SDO response plus target-independent effects caused by a successful write. */
struct SdoServerResult {
    CanFrame frame;
    DictionaryWriteEffects effects;
};

/** Handles local expedited scalar uploads and downloads without block state. */
class CanopenSdoServer {
public:
    /// Binds the wire server to the dictionary whose values it exposes.
    explicit CanopenSdoServer(CanopenObjectDictionary& dictionary);

    /** Handles a complete local SDO request or ignores unrelated CAN frames. */
    std::optional<SdoServerResult> handle(const CanFrame& request);

private:
    /// Creates a standard abort response retaining the requested object address.
    static SdoServerResult abort_response(const CanFrame& request,
                                           SdoAbort abort);

    /// Creates a local response header with the copied object address.
    static CanFrame response_header(const CanFrame& request,
                                    std::uint8_t command);

    CanopenObjectDictionary& dictionary_;
};

}  // namespace firmware::core
