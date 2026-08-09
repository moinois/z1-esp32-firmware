// Verifies that TCP routing decisions reach only their selected service sinks.
#include "test.hpp"
#include "application/transport/tcp_frame_dispatcher.hpp"

TEST_CASE(tcp_dispatcher_routes_local_and_controller_targets) {
    firmware::application::Router router;
    int local_calls = 0;
    int controller_calls = 0;
    firmware::application::TcpDispatchSinks sinks;
    sinks.controller = [&](auto&, const auto&) { ++controller_calls; };
    sinks.local_command = [&](auto&, const auto&) { ++local_calls; };
    firmware::application::TcpFrameDispatcher dispatcher(router, sinks);
    const firmware::core::Frame frame{
        firmware::core::protocol::general_command, {'p', 'l', 'a', 'y'}};
    firmware::application::TcpClientSession session({});
    dispatcher.dispatch(session, frame);
    REQUIRE_EQ(local_calls, 1);
    REQUIRE_EQ(controller_calls, 1);
}

TEST_CASE(tcp_dispatcher_exposes_origin_session_to_local_sink) {
    firmware::application::Router router;
    firmware::application::TcpDispatchSinks sinks;
    sinks.local_command = [](auto& session, const auto&) {
        static_cast<void>(session.queue_frame({0x90U, {'o', 'k'}}));
    };
    firmware::application::TcpFrameDispatcher dispatcher(router, sinks);
    firmware::application::TcpClientSession session({});
    dispatcher.dispatch(session,
        {firmware::core::protocol::single_command, {'?', 'x'}});
    REQUIRE_EQ(session.transmit_queue().size(), 1U);
}

TEST_CASE(tcp_dispatcher_routes_file_data_only_to_file_sink) {
    firmware::application::Router router;
    int file_calls = 0;
    int controller_calls = 0;
    firmware::application::TcpDispatchSinks sinks;
    sinks.controller = [&](auto&, const auto&) { ++controller_calls; };
    sinks.file_transfer = [&](auto&, const auto&) { ++file_calls; };
    firmware::application::TcpFrameDispatcher dispatcher(router, sinks);
    const firmware::application::HostIdentity host{};
    router.ownership().claim_file(host);
    firmware::application::TcpClientSession session(host);
    dispatcher.dispatch(session, {firmware::core::protocol::file_data, {'x'}});
    REQUIRE_EQ(file_calls, 1);
    REQUIRE_EQ(controller_calls, 0);
}
