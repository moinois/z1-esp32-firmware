// Declares controller-update handoff monitoring, failures, and completion.
#pragma once

#include <cstdint>
#include <string_view>

namespace firmware::application {

// Isolates handoff policy from storage, transfer channels, routing, and phase.
class UpdateControllerPort {
public:
    // Enables safe destruction through a substituted controller adapter.
    virtual ~UpdateControllerPort() = default;

    // Reports whether staged controller firmware exists.
    virtual bool staged_controller_exists() const = 0;

    // Reports whether controller firmware transfer is active.
    virtual bool firmware_transfer_active() const = 0;

    // Reports whether controller configuration transfer is active.
    virtual bool configuration_transfer_active() const = 0;

    // Reports whether controller factory-data transfer is active.
    virtual bool factory_transfer_active() const = 0;

    // Sends the exact controller reset command.
    virtual void send_controller_reset() = 0;

    // Publishes volatile update failure phase three.
    virtual void publish_error() = 0;

    // Attempts deletion of the exact staged controller image path.
    virtual void remove_staged_controller(std::string_view path) = 0;

    // Publishes persisted and transient successful controller completion.
    virtual void controller_completed(std::uint64_t now_milliseconds) = 0;
};

// Owns the drift-free controller reset schedule and transfer outcome policy.
class UpdateControllerMonitor {
public:
    // Binds controller handoff rules to replaceable state and transport ports.
    explicit UpdateControllerMonitor(UpdateControllerPort& port);

    // Arms the first staged-content check for immediate execution.
    void start(std::uint64_t now_milliseconds);

    // Runs at most one due check and advances the five-second schedule.
    void tick(std::uint64_t now_milliseconds);

    // Publishes failure for a protocol error when staged content still exists.
    void transfer_failed();

    // Publishes failure for cancellation when staged content still exists.
    void transfer_cancelled();

    // Publishes failure only for a qualifying timeout with staged content.
    void transfer_timed_out(bool qualifying);

    // Deletes staged content and publishes success regardless of deletion.
    void controller_completed(std::uint64_t now_milliseconds);

private:
    // Publishes failure only while a staged controller image is available.
    void publish_failure_if_available();

    // Reports whether all three controller transfer channels are inactive.
    bool transfer_channels_idle() const;

    UpdateControllerPort& port_;
    std::uint64_t next_check_milliseconds_ = 0U;
    bool started_ = false;
};

}  // namespace firmware::application
