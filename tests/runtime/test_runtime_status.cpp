// Tests composition of runtime state into status and diagnostic fields.
#include "test.hpp"

#include "firmware/application/runtime_status.hpp"

#include <optional>

using firmware::application::AggregatedStatusPort;
using firmware::application::AggregatedStatusService;
using firmware::application::SdCapacity;
using firmware::application::UpdateStatus;

namespace {

class FakeAggregatedStatusPort final : public AggregatedStatusPort {
public:
    // Reports whether a host file operation currently owns its worker.
    bool host_transfer_active() const override {
        return transfer_active;
    }

    // Reports the retained recording request flag.
    bool recording_requested() const override {
        return record_requested;
    }

    // Reports whether an AVI segment is currently open for recording.
    bool recording_active() const override {
        return recording;
    }

    // Returns the newest complete SD capacity sample, when available.
    std::optional<SdCapacity> sd_capacity() const override {
        return capacity;
    }

    // Returns the update state currently exposed by the update service.
    UpdateStatus update_status() const override {
        return update;
    }

    // Returns the associated access point's RSSI, when available.
    std::optional<std::int32_t> station_rssi() const override {
        return rssi;
    }

    bool transfer_active = false;
    bool record_requested = false;
    bool recording = false;
    std::optional<SdCapacity> capacity;
    UpdateStatus update{0U, 0U};
    std::optional<std::int32_t> rssi;
};

}  // namespace

TEST_CASE(run_050_status_composes_all_live_sources) {
    FakeAggregatedStatusPort port;
    port.transfer_active = true;
    port.record_requested = true;
    port.recording = true;
    port.capacity = SdCapacity{4096U, 1024U};
    port.update = UpdateStatus{2U, 67U};
    AggregatedStatusService status(port);

    const auto extension = status.extension();

    REQUIRE(extension.transfer_active);
    REQUIRE(extension.record_requested);
    REQUIRE(extension.recording);
    REQUIRE_EQ(extension.sd_used_mib, 3072U);
    REQUIRE_EQ(extension.sd_total_mib, 4096U);
    REQUIRE_EQ(extension.update_phase, 2U);
    REQUIRE_EQ(extension.update_progress, 67U);
}

TEST_CASE(run_051_sd_used_never_underflows_when_free_exceeds_total) {
    FakeAggregatedStatusPort port;
    port.capacity = SdCapacity{100U, 125U};
    AggregatedStatusService status(port);

    const auto extension = status.extension();

    REQUIRE_EQ(extension.sd_used_mib, 0U);
    REQUIRE_EQ(extension.sd_total_mib, 100U);
}

TEST_CASE(run_051_unavailable_sd_uses_named_fallback_for_both_fields) {
    FakeAggregatedStatusPort port;
    AggregatedStatusService status(port);

    const auto extension = status.extension();

    REQUIRE_EQ(extension.sd_used_mib,
               firmware::core::status::unavailable_sd_capacity_mib);
    REQUIRE_EQ(extension.sd_total_mib,
               firmware::core::status::unavailable_sd_capacity_mib);
}

TEST_CASE(run_052_diagnostic_rssi_uses_station_value_or_zero) {
    FakeAggregatedStatusPort port;
    AggregatedStatusService status(port);

    REQUIRE_EQ(status.diagnostic_rssi(), 0);

    port.rssi = -73;
    REQUIRE_EQ(status.diagnostic_rssi(), -73);
}
