// Verifies complete-frame writes and TCP retry behavior.
#include "test.hpp"
#include "firmware/application/tcp_frame_sender.hpp"

TEST_CASE(tcp_007_sender_drains_short_writes) {
    firmware::application::TcpFrameSender sender;
    int calls = 0;
    REQUIRE(sender.send(firmware::core::BytesView("abcd"),
        [&](firmware::core::BytesView remaining) {
            ++calls;
            return firmware::application::TcpSendResult{
                firmware::application::TcpSendStatus::sent,
                remaining.size() > 2U ? 2U : remaining.size()};
        }));
    REQUIRE_EQ(calls, 2);
}

TEST_CASE(tcp_008_sender_retries_temporary_results) {
    firmware::application::TcpFrameSender sender;
    int calls = 0;
    REQUIRE(sender.send(firmware::core::BytesView("ok"),
        [&](firmware::core::BytesView remaining) {
            ++calls;
            if (calls < 3) {
                return firmware::application::TcpSendResult{
                    firmware::application::TcpSendStatus::temporary_failure, 0U};
            }
            return firmware::application::TcpSendResult{
                firmware::application::TcpSendStatus::sent, remaining.size()};
        }));
    REQUIRE_EQ(calls, 3);
}

TEST_CASE(tcp_007_sender_stops_on_permanent_failure) {
    firmware::application::TcpFrameSender sender;
    REQUIRE(!sender.send(firmware::core::BytesView("bad"),
        [](firmware::core::BytesView) {
            return firmware::application::TcpSendResult{
                firmware::application::TcpSendStatus::permanent_failure, 0U};
        }));
}
