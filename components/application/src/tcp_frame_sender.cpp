// Implements whole-frame TCP transmission and retry semantics.
#include "firmware/application/tcp_frame_sender.hpp"

namespace firmware::application {

bool TcpFrameSender::send(core::BytesView frame,
                          const SendFunction& send_function) const {
    if (frame.size() == 0U || !send_function) {
        return false;
    }
    std::size_t offset = 0U;
    while (offset < frame.size()) {
        const core::BytesView remaining(frame.data() + offset,
                                        frame.size() - offset);
        const TcpSendResult result = send_function(remaining);
        if (result.status == TcpSendStatus::temporary_failure) {
            continue;
        }
        if (result.status == TcpSendStatus::permanent_failure
            || result.bytes_sent == 0U
            || result.bytes_sent > remaining.size()) {
            return false;
        }
        offset += result.bytes_sent;
    }
    return true;
}

}  // namespace firmware::application
