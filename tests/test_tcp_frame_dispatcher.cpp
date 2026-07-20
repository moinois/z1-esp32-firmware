// Verifies that TCP routing decisions reach only their selected service sinks.
#include "test.hpp"
#include "firmware/application/tcp_frame_dispatcher.hpp"

TEST_CASE(tcp_dispatcher_routes_local_and_controller_targets) {
    firmware::application::Router router;
    int local_calls = 0;
    int controller_calls = 0;
    firmware::application::TcpFrameDispatcher dispatcher(
        router,
        {.controller = [&](const auto&, const auto&) { ++controller_calls; },
         .local_command = [&](const auto&, const auto&) { ++local_calls; }});
    const firmware::core::Frame frame{
        firmware::core::protocol::general_command, {'p', 'l', 'a', 'y'}};
    dispatcher.dispatch({}, frame);
    REQUIRE_EQ(local_calls, 1);
    REQUIRE_EQ(controller_calls, 1);
}

TEST_CASE(tcp_dispatcher_routes_file_data_only_to_file_sink) {
    firmware::application::Router router;
    int file_calls = 0;
    int controller_calls = 0;
    firmware::application::TcpFrameDispatcher dispatcher(
        router,
        {.controller = [&](const auto&, const auto&) { ++controller_calls; },
         .file_transfer = [&](const auto&, const auto&) { ++file_calls; }});
    const firmware::application::HostIdentity host{};
    router.ownership().claim_file(host);
    dispatcher.dispatch(host,
        {firmware::core::protocol::file_data, {'x'}});
    REQUIRE_EQ(file_calls, 1);
    REQUIRE_EQ(controller_calls, 0);
}
