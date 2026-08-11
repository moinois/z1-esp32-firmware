// Verifies that media capability creation is lazy, shared, and never retried.
#include "test.hpp"

#include "application/runtime/live_initialization.hpp"

using firmware::application::LiveInitialization;
using firmware::application::LiveInitializationPort;

namespace {

class FakeLiveInitializationPort final : public LiveInitializationPort {
public:
    bool initialize_live_media() override {
        ++attempts;
        return succeeds;
    }

    bool succeeds = true;
    unsigned attempts = 0U;
};

}  // namespace

TEST_CASE(live_010_first_affected_request_initializes_media_once) {
    FakeLiveInitializationPort port;
    LiveInitialization initialization(port);

    REQUIRE(initialization.ensure_available());
    REQUIRE(initialization.ensure_available());
    REQUIRE_EQ(port.attempts, 1U);
}

TEST_CASE(live_010_failed_initialization_remains_unavailable_until_reset) {
    FakeLiveInitializationPort port;
    port.succeeds = false;
    LiveInitialization initialization(port);

    REQUIRE(!initialization.ensure_available());
    port.succeeds = true;
    REQUIRE(!initialization.ensure_available());
    REQUIRE_EQ(port.attempts, 1U);
}
