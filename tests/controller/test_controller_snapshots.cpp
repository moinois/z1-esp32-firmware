// Tests bounded controller snapshots and their local host response formats.
#include "test.hpp"

#include "application/controller/controller_snapshots.hpp"

#include <string>

using firmware::application::ControllerSnapshots;
using firmware::core::ByteVector;
using firmware::core::StatusExtension;

namespace {

std::string text(const ByteVector& bytes) {
    return {bytes.begin(), bytes.end()};
}

ByteVector bytes(const std::string& value) {
    return {value.begin(), value.end()};
}

}  // namespace

TEST_CASE(lpc_003_initial_status_contains_four_spaces_before_feed_rate) {
    ControllerSnapshots snapshots;

    const auto reply = snapshots.status_reply({});

    REQUIRE(reply.has_value());
    REQUIRE_EQ(reply->type, 0x81U);
    REQUIRE(text(reply->payload).find("|    F:0.0,3000.0,100.0|") != std::string::npos);
}

TEST_CASE(stat_001_all_controller_snapshots_are_truncated_to_528_bytes) {
    ControllerSnapshots snapshots;
    snapshots.update_status(bytes("<" + std::string(600, 's') + ">"));
    snapshots.update_diagnostic(ByteVector(600, 'd'));
    snapshots.update_version(ByteVector(600, 'v'));

    REQUIRE_EQ(snapshots.latest_status_size(), 528U);
    REQUIRE_EQ(snapshots.diagnostic_size(), 528U);
    REQUIRE_EQ(snapshots.version_size(), 528U);
}

TEST_CASE(stat_002_empty_status_is_ignored_but_other_empty_snapshots_replace) {
    ControllerSnapshots snapshots;
    snapshots.update_status(bytes("<Run>"));
    snapshots.update_diagnostic(bytes("{old}"));
    snapshots.update_version(bytes("old"));

    snapshots.update_status({});
    snapshots.update_diagnostic({});
    snapshots.update_version({});

    REQUIRE(text(snapshots.status_reply({})->payload).find("<Run|") == 0U);
    REQUIRE(!snapshots.diagnostic_reply(-20).has_value());
    REQUIRE_EQ(text(snapshots.version_reply().payload), std::string("version = .0.1.11\n"));
}

TEST_CASE(stat_003_only_three_newest_pending_statuses_are_retained) {
    ControllerSnapshots snapshots;
    snapshots.update_status(bytes("<S1>"));
    snapshots.update_status(bytes("<S2>"));
    snapshots.update_status(bytes("<S3>"));
    snapshots.update_status(bytes("<S4>"));

    REQUIRE_EQ(snapshots.pending_status_count(), 3U);
    const auto reply = snapshots.status_reply({});
    REQUIRE(text(reply->payload).find("<S4|") == 0U);
    REQUIRE_EQ(snapshots.pending_status_count(), 0U);
}

TEST_CASE(stat_004_status_request_uses_latest_after_consuming_pending_values) {
    ControllerSnapshots snapshots;
    snapshots.update_status(bytes("<Latest>"));

    REQUIRE(text(snapshots.status_reply({})->payload).find("<Latest|") == 0U);
    REQUIRE(text(snapshots.status_reply({})->payload).find("<Latest|") == 0U);
}

TEST_CASE(stat_004_malformed_latest_status_uses_exact_fallback) {
    ControllerSnapshots snapshots;
    snapshots.update_status(bytes("malformed"));

    const auto reply = snapshots.status_reply({});

    REQUIRE(reply.has_value());
    REQUIRE(text(reply->payload).find("<Idle|MPos:-1.0000") == 0U);
    REQUIRE(text(reply->payload).find("|WPos:144.4120,158.7000,77.9550,8.0010,0.0000|F:") != std::string::npos);
}

TEST_CASE(stat_005_status_reply_uses_current_local_extension) {
    ControllerSnapshots snapshots;
    snapshots.update_status(bytes("<Idle>"));
    StatusExtension extension;
    extension.transfer_active = true;
    extension.update_phase = 2;
    extension.update_progress = 50;

    REQUIRE_EQ(text(snapshots.status_reply(extension)->payload),
               std::string("<Idle|E:1,0,0,1234,1234|OTA:2,50>\n"));
}

TEST_CASE(stat_007_diagnostic_reply_inserts_signed_rssi_before_first_closing_brace) {
    ControllerSnapshots snapshots;
    snapshots.update_diagnostic(bytes("{A:1}ignored"));

    const auto reply = snapshots.diagnostic_reply(-47);

    REQUIRE(reply.has_value());
    REQUIRE_EQ(reply->type, 0x82U);
    REQUIRE_EQ(text(reply->payload), std::string("{A:1|RSSI:-47}\n"));
}

TEST_CASE(stat_007_diagnostic_requires_opening_and_closing_braces) {
    ControllerSnapshots snapshots;
    snapshots.update_diagnostic(bytes("A:1}"));
    REQUIRE(!snapshots.diagnostic_reply(0).has_value());

    snapshots.update_diagnostic(bytes("{A:1"));
    REQUIRE(!snapshots.diagnostic_reply(0).has_value());
}

TEST_CASE(stat_008_oversized_rssi_result_returns_original_through_one_following_byte) {
    ControllerSnapshots snapshots;
    const std::string diagnostic = "{" + std::string(520, 'x') + "}Ztail";
    snapshots.update_diagnostic(bytes(diagnostic));

    const auto reply = snapshots.diagnostic_reply(-100);

    REQUIRE(reply.has_value());
    REQUIRE_EQ(reply->payload.size(), 523U);
    REQUIRE_EQ(reply->payload[521], static_cast<std::uint8_t>('}'));
    REQUIRE_EQ(reply->payload[522], static_cast<std::uint8_t>('Z'));
}

TEST_CASE(stat_009_version_uses_short_nul_terminated_controller_prefix) {
    ControllerSnapshots snapshots;
    snapshots.update_version(ByteVector{'1', '.', '2', 0, 'x'});

    REQUIRE_EQ(text(snapshots.version_reply().payload), std::string("version = 1.2.0.1.11\n"));
}

TEST_CASE(stat_009_version_omits_prefix_when_it_is_64_bytes_or_longer) {
    ControllerSnapshots snapshots;
    snapshots.update_version(ByteVector(64, 'v'));

    REQUIRE_EQ(text(snapshots.version_reply().payload), std::string("version = .0.1.11\n"));
}
