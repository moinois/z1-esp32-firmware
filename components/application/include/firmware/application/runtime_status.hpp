/** @file @brief Declares replaceable runtime sources and aggregate status composition. */
#pragma once

#include "firmware/application/sd_card_lifecycle.hpp"
#include "firmware/application/update_phase.hpp"
#include "firmware/core/status.hpp"

#include <cstdint>
#include <optional>

namespace firmware::application {

/// Isolates machine-status composition from transfer, media, SD, update, and Wi-Fi services.
class AggregatedStatusPort {
public:
    /// Enables safe destruction through a substituted status-source adapter.
    virtual ~AggregatedStatusPort() = default;

    /// Reports whether a host file operation currently owns its worker.
    virtual bool host_transfer_active() const = 0;

    /// Reports whether recording has been requested by a host command.
    virtual bool recording_requested() const = 0;

    /// Reports whether an AVI segment is currently open for recording.
    virtual bool recording_active() const = 0;

    /// Returns the newest complete SD capacity sample, when available.
    virtual std::optional<SdCapacity> sd_capacity() const = 0;

    /// Returns the update state currently exposed by the update service.
    virtual UpdateStatus update_status() const = 0;

    /// Returns the associated access point's signed RSSI, when available.
    virtual std::optional<std::int32_t> station_rssi() const = 0;
};

/// Converts independent runtime sources into controller-snapshot reply fields.
class AggregatedStatusService {
public:
    /// Binds composition to a replaceable collection of runtime sources.
    explicit AggregatedStatusService(const AggregatedStatusPort& port);

    /// Builds one current extension for a machine-status response.
    core::StatusExtension extension() const;

    /// Returns associated RSSI, or zero when no associated value is available.
    std::int32_t diagnostic_rssi() const;

private:
    const AggregatedStatusPort& port_;
};

}  // namespace firmware::application
