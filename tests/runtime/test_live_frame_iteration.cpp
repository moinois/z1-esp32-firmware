// Verifies LIVE-011 and LIVE-012 validation, capture, and send ordering.
#include "test.hpp"

#include "application/runtime/live_frame_iteration.hpp"

#include <optional>
#include <string>
#include <vector>

using firmware::application::LiveFramePort;
using firmware::core::ByteVector;

namespace {

class FakeLiveFramePort final : public LiveFramePort {
public:
    bool socket_valid(std::uint32_t socket_id) override {
        events.push_back("valid");
        observed_socket = socket_id;
        ++validation_count;
        return valid;
    }

    std::optional<ByteVector> capture_jpeg() override {
        events.push_back("capture");
        valid = valid_after_capture;
        return capture_succeeds ? std::optional<ByteVector>({1U, 2U, 3U})
                                : std::nullopt;
    }

    bool send_jpeg(std::uint32_t socket_id,
                   firmware::core::BytesView frame) override {
        events.push_back("send");
        observed_socket = socket_id;
        sent.assign(frame.begin(), frame.end());
        return send_succeeds;
    }

    bool valid = true;
    bool valid_after_capture = true;
    bool capture_succeeds = true;
    bool send_succeeds = true;
    std::size_t validation_count = 0U;
    std::uint32_t observed_socket = 0U;
    ByteVector sent;
    std::vector<std::string> events;
};

}  // namespace

TEST_CASE(live_011_invalid_claimed_socket_terminates_before_capture) {
    FakeLiveFramePort port;
    port.valid = false;

    REQUIRE(!firmware::application::run_live_frame_iteration(42U, port));
    REQUIRE_EQ(port.events, std::vector<std::string>({"valid"}));
    REQUIRE(port.sent.empty());
}

TEST_CASE(live_012_socket_is_not_revalidated_between_capture_and_send) {
    FakeLiveFramePort port;
    port.valid_after_capture = false;

    REQUIRE(firmware::application::run_live_frame_iteration(7U, port));
    REQUIRE_EQ(port.events,
               std::vector<std::string>({"valid", "capture", "send"}));
    REQUIRE_EQ(port.validation_count, 1U);
    REQUIRE_EQ(port.observed_socket, 7U);
    REQUIRE_EQ(port.sent, ByteVector({1U, 2U, 3U}));
}

TEST_CASE(live_012_capture_or_send_failure_terminates_the_iteration) {
    FakeLiveFramePort capture_failure;
    capture_failure.capture_succeeds = false;
    REQUIRE(!firmware::application::run_live_frame_iteration(1U, capture_failure));
    REQUIRE_EQ(capture_failure.events,
               std::vector<std::string>({"valid", "capture"}));

    FakeLiveFramePort send_failure;
    send_failure.send_succeeds = false;
    REQUIRE(!firmware::application::run_live_frame_iteration(1U, send_failure));
    REQUIRE_EQ(send_failure.events,
               std::vector<std::string>({"valid", "capture", "send"}));
}
