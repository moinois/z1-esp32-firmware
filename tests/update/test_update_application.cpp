// Verifies validated package staging, OTA failure handling, and final actions.
#include "test.hpp"

#include "firmware/application/update_application.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::UpdateApplicationPort;
using firmware::application::UpdateApplicationService;
using firmware::application::ValidatedUpdatePackage;
using firmware::core::ByteVector;
using firmware::core::UpdateHeader;

namespace {

// Selects one replaceable OTA operation to fail in an application test.
enum class FailedOperation {
    none,
    select_partition,
    begin,
    write,
    finalize,
    set_boot,
};

// Records all update staging, flash, persistence, deletion, and restart calls.
class FakeUpdateApplicationPort final : public UpdateApplicationPort {
public:
    // Records one volatile/pending phase publication.
    void publish_phase(std::uint8_t phase) override {
        calls.emplace_back("publish");
        published_phases.push_back(phase);
    }

    // Selects the inactive partition unless configured to fail.
    bool select_inactive_partition() override {
        calls.emplace_back("select");
        return failed_operation != FailedOperation::select_partition;
    }

    // Starts an OTA write for the exact declared mainboard byte count.
    bool begin_mainboard_write(std::uint32_t size) override {
        calls.emplace_back("begin");
        begin_sizes.push_back(size);
        return failed_operation != FailedOperation::begin;
    }

    // Writes the exact declared mainboard image bytes.
    bool write_mainboard(firmware::core::BytesView image) override {
        calls.emplace_back("write");
        mainboard_images.emplace_back(image.begin(), image.end());
        return failed_operation != FailedOperation::write;
    }

    // Finalizes the active OTA image unless configured to fail.
    bool finalize_mainboard_write() override {
        calls.emplace_back("finalize");
        return failed_operation != FailedOperation::finalize;
    }

    // Selects the newly written application as the boot target.
    bool select_mainboard_for_boot() override {
        calls.emplace_back("set-boot");
        return failed_operation != FailedOperation::set_boot;
    }

    // Aborts one still-active OTA write.
    void abort_mainboard_write() override {
        calls.emplace_back("abort");
    }

    // Best-effort stages one exact controller image with one write.
    void stage_controller(std::string_view path,
                          firmware::core::BytesView image) override {
        calls.emplace_back("stage-controller");
        controller_paths.emplace_back(path);
        controller_images.emplace_back(image.begin(), image.end());
    }

    // Directly persists phase two after a completed mainboard image.
    void persist_phase_direct(std::uint8_t phase) override {
        calls.emplace_back("persist-direct");
        direct_phases.push_back(phase);
    }

    // Best-effort deletes the consumed aggregate.
    void remove_aggregate(std::string_view path) override {
        calls.emplace_back("remove-aggregate");
        removed_paths.emplace_back(path);
    }

    // Sends the exact reset command when the mainboard does not restart.
    void send_controller_reset() override {
        calls.emplace_back("controller-reset");
    }

    // Restarts the mainboard after successful OTA application.
    void restart_mainboard() override {
        calls.emplace_back("restart");
    }

    FailedOperation failed_operation = FailedOperation::none;
    std::vector<std::string> calls;
    std::vector<std::uint8_t> published_phases;
    std::vector<std::uint32_t> begin_sizes;
    std::vector<ByteVector> mainboard_images;
    std::vector<ByteVector> controller_images;
    std::vector<std::string> controller_paths;
    std::vector<std::string> removed_paths;
    std::vector<std::uint8_t> direct_phases;
};

// Builds one already-validated aggregate with the requested image bytes.
ValidatedUpdatePackage package(ByteVector mainboard, ByteVector controller) {
    ByteVector bytes(32U, 0U);
    bytes.insert(bytes.end(), mainboard.begin(), mainboard.end());
    bytes.insert(bytes.end(), controller.begin(), controller.end());
    UpdateHeader header;
    header.mainboard_size = static_cast<std::uint32_t>(mainboard.size());
    header.controller_size = static_cast<std::uint32_t>(controller.size());
    return {header, std::move(bytes)};
}

}  // namespace

TEST_CASE(upd_030_controller_only_stages_then_deletes_and_resets) {
    FakeUpdateApplicationPort port;
    UpdateApplicationService application(port);

    REQUIRE(application.apply(package({}, {1U, 2U, 3U})));

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"stage-controller",
                                         "remove-aggregate",
                                         "controller-reset"}));
    REQUIRE_EQ(port.controller_images,
               std::vector<ByteVector>({{1U, 2U, 3U}}));
    REQUIRE_EQ(port.controller_paths,
               std::vector<std::string>({"/sd/lpc1768.bin"}));
    REQUIRE_EQ(port.removed_paths,
               std::vector<std::string>({"/sd/firmware.bin"}));
}

TEST_CASE(upd_030_empty_package_deletes_and_resets_without_staging) {
    FakeUpdateApplicationPort port;
    UpdateApplicationService application(port);

    REQUIRE(application.apply(package({}, {})));

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"remove-aggregate",
                                         "controller-reset"}));
}

TEST_CASE(upd_032_to_034_mainboard_success_uses_exact_order_and_images) {
    FakeUpdateApplicationPort port;
    UpdateApplicationService application(port);

    REQUIRE(application.apply(package({0xE9U, 1U}, {7U, 8U})));

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({
                   "publish", "select", "begin", "write", "finalize",
                   "set-boot", "stage-controller", "persist-direct",
                   "remove-aggregate", "restart"}));
    REQUIRE_EQ(port.published_phases, std::vector<std::uint8_t>({1U}));
    REQUIRE_EQ(port.begin_sizes, std::vector<std::uint32_t>({2U}));
    REQUIRE_EQ(port.mainboard_images,
               std::vector<ByteVector>({{0xE9U, 1U}}));
    REQUIRE_EQ(port.controller_images,
               std::vector<ByteVector>({{7U, 8U}}));
    REQUIRE_EQ(port.direct_phases, std::vector<std::uint8_t>({2U}));
}

TEST_CASE(upd_033_pre_finalize_failures_abort_only_an_active_write) {
    for (const FailedOperation failure :
         {FailedOperation::select_partition, FailedOperation::begin,
          FailedOperation::write, FailedOperation::finalize}) {
        FakeUpdateApplicationPort port;
        port.failed_operation = failure;
        UpdateApplicationService application(port);

        REQUIRE(!application.apply(package({0xE9U}, {})));

        REQUIRE_EQ(port.published_phases,
                   std::vector<std::uint8_t>({1U, 3U}));
        const bool should_abort = failure == FailedOperation::write ||
                                  failure == FailedOperation::finalize;
        REQUIRE_EQ(std::find(port.calls.begin(), port.calls.end(), "abort") !=
                       port.calls.end(),
                   should_abort);
        REQUIRE(std::find(port.calls.begin(), port.calls.end(),
                          "remove-aggregate") == port.calls.end());
        REQUIRE(std::find(port.calls.begin(), port.calls.end(), "restart") ==
                port.calls.end());
    }
}

TEST_CASE(upd_033_boot_selection_failure_has_no_active_write_to_abort) {
    FakeUpdateApplicationPort port;
    port.failed_operation = FailedOperation::set_boot;
    UpdateApplicationService application(port);

    REQUIRE(!application.apply(package({0xE9U}, {7U})));

    REQUIRE_EQ(port.published_phases,
               std::vector<std::uint8_t>({1U, 3U}));
    REQUIRE(std::find(port.calls.begin(), port.calls.end(), "abort") ==
            port.calls.end());
    REQUIRE(port.controller_images.empty());
    REQUIRE(port.direct_phases.empty());
}
