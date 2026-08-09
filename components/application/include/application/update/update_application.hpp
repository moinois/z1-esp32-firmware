/** @file @brief Declares application of validated aggregate mainboard and controller images. */
#pragma once

#include "application/update/update_validation.hpp"
#include "core/protocol/bytes.hpp"

#include <cstdint>
#include <string_view>

namespace firmware::application {

/// Isolates update application from OTA, storage, phase, controller, and reset.
class UpdateApplicationPort {
public:
    /// Enables safe destruction through a substituted application adapter.
    virtual ~UpdateApplicationPort() = default;

    /// Publishes a volatile phase and requests queued persistence.
    virtual void publish_phase(std::uint8_t phase) = 0;

    /// Selects the inactive application partition for an OTA write.
    virtual bool select_inactive_partition() = 0;

    /// Begins one OTA write for the exact declared image size.
    virtual bool begin_mainboard_write(std::uint32_t size) = 0;

    /// Writes the exact declared mainboard image bytes.
    virtual bool write_mainboard(core::BytesView image) = 0;

    /// Finalizes and validates the active OTA write.
    virtual bool finalize_mainboard_write() = 0;

    /// Selects the finalized application partition for the next boot.
    virtual bool select_mainboard_for_boot() = 0;

    /// Aborts one still-active OTA write after failure.
    virtual void abort_mainboard_write() = 0;

    /// Best-effort truncates and writes the staged controller image.
    virtual void stage_controller(std::string_view path,
                                  core::BytesView image) = 0;

    /// Directly persists phase two after successful mainboard application.
    virtual void persist_phase_direct(std::uint8_t phase) = 0;

    /// Best-effort removes the consumed aggregate from its exact path.
    virtual void remove_aggregate(std::string_view path) = 0;

    /// Sends the controller reset command after controller-only application.
    virtual void send_controller_reset() = 0;

    /// Restarts the mainboard after successful OTA application.
    virtual void restart_mainboard() = 0;
};

/// Applies one validated package under exact failure and finalization ordering.
class UpdateApplicationService {
public:
    /// Binds deterministic application policy to replaceable outer operations.
    explicit UpdateApplicationService(UpdateApplicationPort& port);

    /// Applies one package and reports whether its terminal handoff succeeded.
    bool apply(const ValidatedUpdatePackage& package);

private:
    /// Best-effort stages the declared controller portion when nonempty.
    void stage_controller(const ValidatedUpdatePackage& package);

    /// Aborts active OTA state when needed and publishes phase three.
    bool fail_mainboard(bool write_active);

    UpdateApplicationPort& port_;
};

}  // namespace firmware::application
