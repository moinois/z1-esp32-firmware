/** @file @brief Transport-independent whole-frame TCP send policy. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstddef>
#include <functional>

namespace firmware::application {

/** Outcome of one nonblocking socket send attempt. */
enum class TcpSendStatus { sent, temporary_failure, permanent_failure };

/** Socket outcome paired with the accepted prefix length. */
struct TcpSendResult {
    TcpSendStatus status;
    std::size_t bytes_sent;
};

/** Completes frames across short writes and transient socket failures. */
class TcpFrameSender {
public:
    /// Callback receiving only the currently unsent suffix.
    using SendFunction = std::function<TcpSendResult(core::BytesView)>;

    /** Sends a complete frame, retrying temporary failures indefinitely.
     *  @return False only after a permanent failure or invalid send result.
     */
    bool send(core::BytesView frame, const SendFunction& send_function) const;
};

}  // namespace firmware::application
