// Tests packet routing and independent host ownership at the application boundary.
#include "test.hpp"

#include "firmware/application/ownership.hpp"
#include "firmware/application/router.hpp"

using firmware::application::ControllerFamily;
using firmware::application::HostIdentity;
using firmware::application::HostTransport;
using firmware::application::Ownership;
using firmware::application::RouteTarget;
using firmware::application::Router;
using firmware::application::file_owner_limit_message;
using firmware::core::Frame;

namespace {

HostIdentity tcp_host(std::uint8_t slot, std::uint32_t generation = 1) {
    return {HostTransport::tcp, slot, generation};
}

HostIdentity usb_host(std::uint32_t generation = 1) {
    return {HostTransport::usb, 0, generation};
}

}  // namespace

TEST_CASE(route_001_controller_transfer_families_are_consumed_locally) {
    const Router router;
    const auto firmware = router.from_controller(Frame{0xC9, {}});
    const auto configuration = router.from_controller(Frame{0xD0, {}});
    const auto factory = router.from_controller(Frame{0xEF, {}});
    const auto play = router.from_controller(Frame{0xF8, {}});

    REQUIRE_EQ(firmware.controller_family, ControllerFamily::firmware);
    REQUIRE_EQ(configuration.controller_family, ControllerFamily::configuration);
    REQUIRE_EQ(factory.controller_family, ControllerFamily::factory_data);
    REQUIRE_EQ(play.controller_family, ControllerFamily::streamed_play);
    REQUIRE(!firmware.has(RouteTarget::broadcast));
}

TEST_CASE(route_002_controller_snapshots_are_not_broadcast_directly) {
    const Router router;

    REQUIRE(router.from_controller(Frame{0x81, {'s'}}).has(RouteTarget::status_snapshot));
    REQUIRE(router.from_controller(Frame{0x82, {'d'}}).has(RouteTarget::diagnostic_snapshot));
    REQUIRE(router.from_controller(Frame{0x71, {'v'}}).has(RouteTarget::version_snapshot));
    REQUIRE(!router.from_controller(Frame{0x81, {'s'}}).has(RouteTarget::broadcast));
}

TEST_CASE(route_004_other_controller_frames_are_broadcast_unchanged) {
    const Router router;
    const auto decision = router.from_controller(Frame{0x90, {'x'}});

    REQUIRE(decision.has(RouteTarget::broadcast));
    REQUIRE_EQ(decision.controller_family, ControllerFamily::none);
}

TEST_CASE(route_010_file_start_and_owner_data_enter_file_transfer) {
    Router router;
    const auto owner = tcp_host(0);
    router.ownership().claim_file(owner);

    REQUIRE(router.from_host(owner, Frame{0xB0, {'u', 'p', 'l', 'o', 'a', 'd', ' '}}).has(RouteTarget::file_transfer));
    REQUIRE(router.from_host(owner, Frame{0xB3, {}}).has(RouteTarget::file_transfer));
}

TEST_CASE(route_011_non_owner_file_packets_are_consumed_silently) {
    Router router;
    router.ownership().claim_file(tcp_host(0));
    const auto decision = router.from_host(tcp_host(1), Frame{0xB3, {}});

    REQUIRE(decision.has(RouteTarget::consume));
    REQUIRE(!decision.has(RouteTarget::controller));
    REQUIRE(!decision.has(RouteTarget::file_transfer));
}

TEST_CASE(route_012_play_is_local_and_conditionally_forwarded) {
    Router router;
    const auto decision = router.from_host(tcp_host(0), Frame{0xA2, {'p', 'l', 'a', 'y', ' ', 'x'}});

    REQUIRE(decision.has(RouteTarget::local_command));
    REQUIRE(decision.has(RouteTarget::controller));
    REQUIRE(decision.controller_requires_local_acceptance);
}

TEST_CASE(route_013_play_status_is_local_only) {
    Router router;
    const auto decision = router.from_host(tcp_host(0), Frame{0xB7, {}});

    REQUIRE(decision.has(RouteTarget::play_status));
    REQUIRE(!decision.has(RouteTarget::controller));
}

TEST_CASE(route_014_single_command_status_is_local_and_others_forward) {
    Router router;

    REQUIRE(router.from_host(tcp_host(0), Frame{0xA1, {'?'}}).has(RouteTarget::local_command));
    REQUIRE(router.from_host(tcp_host(0), Frame{0xA1, {'G'}}).has(RouteTarget::controller));
}

TEST_CASE(route_015_m942_is_local_and_forwarded_independently) {
    Router router;
    const auto decision = router.from_host(tcp_host(0), Frame{0xA2, {'M', '9', '4', '2'}});

    REQUIRE(decision.has(RouteTarget::local_command));
    REQUIRE(decision.has(RouteTarget::controller));
    REQUIRE(!decision.controller_requires_local_acceptance);
}

TEST_CASE(route_016_recognized_general_commands_are_local_and_unknown_packets_forward) {
    Router router;

    REQUIRE(router.from_host(tcp_host(0), Frame{0xA2, {'v', 'e', 'r', 's', 'i', 'o', 'n'}})
                .has(RouteTarget::local_command));
    REQUIRE(router.from_host(tcp_host(0), Frame{0xA2, {'G', '0'}}).has(RouteTarget::controller));
    REQUIRE(router.from_host(tcp_host(0), Frame{0x77, {}}).has(RouteTarget::controller));
}

TEST_CASE(route_017_active_controller_transfer_suppresses_only_ordinary_forwarding) {
    Router router;
    router.set_controller_transfer_active(true);

    REQUIRE(router.from_host(tcp_host(0), Frame{0x77, {}}).has(RouteTarget::consume));
    REQUIRE(router.from_host(tcp_host(0), Frame{0xA2, {'v', 'e', 'r', 's', 'i', 'o', 'n'}})
                .has(RouteTarget::local_command));
}

TEST_CASE(route_018_controller_admission_is_silent_and_independent_from_local_handling) {
    Router router;
    auto oversized = router.from_host(tcp_host(0), Frame{0xA2, {'M', '9', '4', '2'}});
    Router::apply_controller_admission(oversized, 545U, true);

    REQUIRE(oversized.has(RouteTarget::local_command));
    REQUIRE(!oversized.has(RouteTarget::controller));

    auto saturated = router.from_host(tcp_host(0), Frame{0x77, {}});
    Router::apply_controller_admission(saturated, 9U, false);
    REQUIRE(saturated.has(RouteTarget::consume));
    REQUIRE(!saturated.has(RouteTarget::controller));

    auto accepted = router.from_host(tcp_host(0), Frame{0x77, {}});
    Router::apply_controller_admission(accepted, 544U, true);
    REQUIRE(accepted.has(RouteTarget::controller));
}

TEST_CASE(own_002_file_claim_allows_same_owner_and_rejects_competitor) {
    Ownership ownership;

    REQUIRE(ownership.claim_file(tcp_host(0)));
    REQUIRE(ownership.claim_file(tcp_host(0)));
    REQUIRE(!ownership.claim_file(tcp_host(1)));
}

TEST_CASE(own_003_file_competition_message_is_byte_exact) {
    REQUIRE_EQ(std::string(file_owner_limit_message),
               std::string("Other client is currently uploading/downloading files. Please try again later."));
}

TEST_CASE(own_008_file_identity_survives_tcp_and_usb_reconnection) {
    Ownership ownership;
    REQUIRE(ownership.claim_file(tcp_host(2, 10)));

    ownership.transport_disconnected(tcp_host(2, 10));
    REQUIRE(ownership.is_file_owner(tcp_host(2, 11)));
    ownership.release_file();

    REQUIRE(ownership.claim_file(usb_host(3)));
    ownership.transport_disconnected(usb_host(3));
    REQUIRE(ownership.is_file_owner(usb_host(4)));
}

TEST_CASE(own_005_play_is_single_client_and_disconnect_releases_it) {
    Ownership ownership;
    REQUIRE(ownership.claim_play(tcp_host(0, 7)));
    REQUIRE(!ownership.claim_play(tcp_host(1, 1)));

    ownership.transport_disconnected(tcp_host(0, 7));
    REQUIRE(!ownership.has_play_owner());
    REQUIRE(ownership.claim_play(tcp_host(1, 1)));
}

TEST_CASE(own_001_file_and_play_ownership_are_independent) {
    Ownership ownership;

    REQUIRE(ownership.claim_file(tcp_host(0)));
    REQUIRE(ownership.claim_play(tcp_host(1)));
    REQUIRE(ownership.is_file_owner(tcp_host(0)));
    REQUIRE(ownership.is_play_owner(tcp_host(1)));
}
