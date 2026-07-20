// Declares transport-independent whole-frame TCP send policy.
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <functional>

namespace firmware::application {

enum class TcpSendStatus { sent, temporary_failure, permanent_failure };

struct TcpSendResult {
    TcpSendStatus status;
    std::size_t bytes_sent;
};

class TcpFrameSender {
public:
    using SendFunction = std::function<TcpSendResult(core::BytesView)>;

    // Sends one frame completely, retrying temporary failures indefinitely.
    bool send(core::BytesView frame, const SendFunction& send_function) const;
};

}  // namespace firmware::application
