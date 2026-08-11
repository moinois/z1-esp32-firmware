// Verifies direct OTA update ordering and exact HTTP outcomes.
#include "test.hpp"

#include "application/update/direct_application_update.hpp"

#include <string>
#include <string_view>
#include <vector>

using firmware::application::DirectApplicationUpdatePort;
using firmware::application::DirectApplicationUpdateService;
using firmware::core::ByteVector;

namespace {

class FakeDirectUpdatePort final : public DirectApplicationUpdatePort {
public:
    bool deinitialize_camera() override {
        events.emplace_back("camera");
        return camera_ok;
    }

    bool select_inactive_partition() override {
        events.emplace_back("partition");
        return partition_ok;
    }

    bool erase_partition() override {
        events.emplace_back("erase");
        return erase_ok;
    }

    bool begin_update(std::size_t size) override {
        events.emplace_back("begin:" + std::to_string(size));
        return begin_ok;
    }

    bool write_image(firmware::core::BytesView image) override {
        events.emplace_back("write:" + std::to_string(image.size()));
        return write_ok;
    }

    bool finish_update() override {
        events.emplace_back("finish");
        return finish_ok;
    }

    bool select_boot_partition() override {
        events.emplace_back("boot");
        return boot_ok;
    }

    void abort_update() override {
        events.emplace_back("abort");
    }

    void send_response(std::uint16_t status, std::string_view body) override {
        response_status = status;
        response_body = body;
        events.emplace_back("response");
    }

    void delay_milliseconds(std::uint32_t milliseconds) override {
        delay = milliseconds;
        events.emplace_back("delay");
    }

    void restart() override {
        events.emplace_back("restart");
    }

    bool camera_ok = true;
    bool partition_ok = true;
    bool erase_ok = true;
    bool begin_ok = true;
    bool write_ok = true;
    bool finish_ok = true;
    bool boot_ok = true;
    std::vector<std::string> events;
    std::uint16_t response_status = 0U;
    std::string_view response_body{};
    std::uint32_t delay = 0U;
};

}  // namespace

TEST_CASE(webup_010_success_uses_exact_ota_order_and_restart_delay) {
    FakeDirectUpdatePort port;
    DirectApplicationUpdateService service;
    const ByteVector image(32U, 0xA5U);
    REQUIRE(service.apply(image, port));
    REQUIRE_EQ(port.events,
               std::vector<std::string>({"camera", "partition", "erase", "begin:4294967295",
                                         "write:32", "finish", "boot", "response",
                                         "delay", "restart"}));
    REQUIRE_EQ(port.response_status, 200U);
    REQUIRE_EQ(port.response_body,
               std::string_view("Firmware upgrade finished. The system will reboot in 2 seconds..."));
    REQUIRE_EQ(port.delay, 2000U);
}

TEST_CASE(webup_011_partition_and_erase_failures_have_exact_500_text) {
    FakeDirectUpdatePort missing;
    missing.partition_ok = false;
    DirectApplicationUpdateService service;
    REQUIRE(!service.apply(ByteVector({'x'}), missing));
    REQUIRE_EQ(missing.response_status, 500U);
    REQUIRE_EQ(missing.response_body, std::string_view("No valid OTA partition detected"));

    FakeDirectUpdatePort erase;
    erase.erase_ok = false;
    REQUIRE(!service.apply(ByteVector({'x'}), erase));
    REQUIRE_EQ(erase.response_body, std::string_view("OTA partition wipe unsuccessful"));
}

TEST_CASE(webup_012_write_failure_is_deferred_to_finalization) {
    FakeDirectUpdatePort port;
    port.write_ok = false;
    DirectApplicationUpdateService service;
    REQUIRE(service.apply(ByteVector({'x', 'y'}), port));
    REQUIRE_EQ(port.response_status, 200U);
    REQUIRE_EQ(port.events[5], std::string("finish"));
}

TEST_CASE(webup_014_empty_or_invalid_finalization_uses_finish_failure) {
    FakeDirectUpdatePort empty;
    DirectApplicationUpdateService service;
    REQUIRE(!service.apply(ByteVector{}, empty));
    REQUIRE_EQ(empty.response_body, std::string_view("OTA finish Failed"));
}

TEST_CASE(webup_011_camera_and_begin_failures_stop_before_writing) {
    DirectApplicationUpdateService service;

    FakeDirectUpdatePort camera;
    camera.camera_ok = false;
    REQUIRE(!service.apply(ByteVector({'x'}), camera));
    REQUIRE_EQ(camera.events,
               std::vector<std::string>({"camera", "response"}));
    REQUIRE_EQ(camera.response_body, std::string_view("OTA finish Failed"));

    FakeDirectUpdatePort begin;
    begin.begin_ok = false;
    REQUIRE(!service.apply(ByteVector({'x'}), begin));
    REQUIRE_EQ(begin.events,
               std::vector<std::string>({"camera", "partition", "erase",
                                         "begin:4294967295", "response"}));
    REQUIRE_EQ(begin.response_body,
               std::string_view("Unable to initialize OTA process"));
}

TEST_CASE(webup_011_finish_and_boot_failures_do_not_restart) {
    DirectApplicationUpdateService service;

    FakeDirectUpdatePort finish;
    finish.finish_ok = false;
    REQUIRE(!service.apply(ByteVector({'x'}), finish));
    REQUIRE_EQ(finish.events,
               std::vector<std::string>({"camera", "partition", "erase",
                                         "begin:4294967295", "write:1", "finish",
                                         "abort", "response"}));
    REQUIRE_EQ(finish.response_body, std::string_view("OTA finish Failed"));

    FakeDirectUpdatePort boot;
    boot.boot_ok = false;
    REQUIRE(!service.apply(ByteVector({'x'}), boot));
    REQUIRE_EQ(boot.events,
               std::vector<std::string>({"camera", "partition", "erase",
                                         "begin:4294967295", "write:1", "finish",
                                         "boot", "response"}));
    REQUIRE_EQ(boot.response_body,
               std::string_view("Failed to set boot partition"));
}

TEST_CASE(webup_004_begin_precedes_content_and_blocks_are_offered_separately) {
    FakeDirectUpdatePort port;
    DirectApplicationUpdateService service;
    REQUIRE(service.begin(port));
    REQUIRE_EQ(port.events,
               std::vector<std::string>({"camera", "partition", "erase",
                                         "begin:4294967295"}));
    service.offer(ByteVector{'a'}, port);
    service.offer(ByteVector{'b', 'c'}, port);
    REQUIRE(service.finish(true, port));
    REQUIRE_EQ(port.events[4], std::string("write:1"));
    REQUIRE_EQ(port.events[5], std::string("write:2"));
}
