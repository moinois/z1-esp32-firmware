// Verifies direct SPIFFS update erase/write/restart policy.
#include "test.hpp"

#include "application/update/direct_web_volume_update.hpp"

#include <string_view>
#include <vector>

using firmware::application::DirectWebVolumeUpdatePort;
using firmware::application::DirectWebVolumeUpdateService;
using firmware::core::ByteVector;

namespace {

class FakeWebVolumeUpdatePort final : public DirectWebVolumeUpdatePort {
public:
    std::optional<std::size_t> partition_size() override {
        events.push_back("partition");
        if (!partition_present) {
            return std::nullopt;
        }
        return capacity;
    }

    bool erase_partition() override {
        events.push_back("erase");
        return erase_ok;
    }

    bool write_content(firmware::core::BytesView content) override {
        events.push_back("write");
        written.assign(content.begin(), content.end());
        return write_ok;
    }

    void send_response(std::uint16_t status, std::string_view body) override {
        response_status = status;
        response_body = body;
        events.push_back("response");
    }

    void delay_milliseconds(std::uint32_t milliseconds) override {
        delay = milliseconds;
        events.push_back("delay");
    }

    void restart() override {
        events.push_back("restart");
    }

    bool partition_present = true;
    bool erase_ok = true;
    bool write_ok = true;
    std::size_t capacity = 100U;
    ByteVector written;
    std::vector<std::string> events;
    std::uint16_t response_status = 0U;
    std::string_view response_body{};
    std::uint32_t delay = 0U;
};

}  // namespace

TEST_CASE(webup_020_and_022_success_writes_from_offset_zero_and_restarts) {
    FakeWebVolumeUpdatePort port;
    DirectWebVolumeUpdateService service;
    const ByteVector content({'a', 'b'});
    REQUIRE(service.apply(content, port));
    REQUIRE_EQ(port.events,
               std::vector<std::string>({"partition", "erase", "write", "response",
                                         "delay", "restart"}));
    REQUIRE_EQ(port.written, content);
    REQUIRE_EQ(port.response_status, 200U);
    REQUIRE_EQ(port.response_body,
               std::string_view("UI upgrade finished. The system will reboot in 2 seconds..."));
    REQUIRE_EQ(port.delay, 2000U);
}

TEST_CASE(webup_021_missing_and_erase_failures_have_exact_500_text) {
    FakeWebVolumeUpdatePort missing;
    missing.partition_present = false;
    DirectWebVolumeUpdateService service;
    REQUIRE(!service.apply(ByteVector{'x'}, missing));
    REQUIRE_EQ(missing.response_status, 500U);
    REQUIRE_EQ(missing.response_body, std::string_view("SPIFFS partition not found"));

    FakeWebVolumeUpdatePort erase;
    erase.erase_ok = false;
    REQUIRE(!service.apply(ByteVector{'x'}, erase));
    REQUIRE_EQ(erase.response_body, std::string_view("SPIFFS erase failed"));
}

TEST_CASE(webup_023_empty_content_is_successful_and_not_validated) {
    FakeWebVolumeUpdatePort port;
    DirectWebVolumeUpdateService service;
    REQUIRE(service.apply(ByteVector{}, port));
    REQUIRE(port.written.empty());
    REQUIRE_EQ(port.response_status, 200U);
}

TEST_CASE(webup_020_oversized_content_does_not_write_or_restart) {
    FakeWebVolumeUpdatePort port;
    port.capacity = 1U;
    DirectWebVolumeUpdateService service;
    REQUIRE(!service.apply(ByteVector{'x', 'y'}, port));
    REQUIRE(port.written.empty());
    REQUIRE_EQ(port.response_status, 0U);
}
