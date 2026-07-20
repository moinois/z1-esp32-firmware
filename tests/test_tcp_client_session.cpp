// Verifies per-client identity, incremental decoding, and transmit composition.
#include "test.hpp"
#include "firmware/application/tcp_client_session.hpp"

TEST_CASE(tcp_session_delivers_complete_frames_with_identity) {
    const firmware::application::HostIdentity identity{
        firmware::application::HostTransport::tcp, 2U, 9U};
    firmware::application::TcpClientSession session(identity);
    const firmware::core::Frame expected{0x42U, {'o', 'k'}};
    const auto encoded = firmware::core::encode_frame(expected);
    firmware::application::HostIdentity received_identity{};
    firmware::core::Frame received_frame{};
    int calls = 0;
    session.receive({encoded.data(), 2U},
        [&](const firmware::application::HostIdentity& received,
            const firmware::core::Frame& frame) {
            received_identity = received;
            received_frame = frame;
            ++calls;
        });
    session.receive({encoded.data() + 2U, encoded.size() - 2U},
        [&](const firmware::application::HostIdentity& received,
            const firmware::core::Frame& frame) {
            received_identity = received;
            received_frame = frame;
            ++calls;
        });
    REQUIRE_EQ(calls, 1);
    REQUIRE_EQ(received_identity, identity);
    REQUIRE_EQ(received_frame, expected);
}

TEST_CASE(tcp_session_queues_encoded_frames) {
    firmware::application::TcpClientSession session({});
    const firmware::core::Frame frame{0x33U, {'x'}};
    REQUIRE(session.queue_frame(frame));
    REQUIRE(session.transmit_queue().front() != nullptr);
    REQUIRE_EQ(*session.transmit_queue().front(), firmware::core::encode_frame(frame));
}
