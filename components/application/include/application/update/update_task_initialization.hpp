/** @file @brief Declares retry policy for aggregate-update processing startup. */
#pragma once

#include <atomic>

namespace firmware::application {

/// Isolates task creation, diagnostics, and request delivery from startup policy.
class UpdateTaskInitializationPort {
public:
    virtual ~UpdateTaskInitializationPort() = default;

    /// Attempts to create all resources needed to process update requests.
    virtual bool start_processing() = 0;
    /// Reports that a request found update processing unavailable.
    virtual void warn_not_started() = 0;
    /// Reports successful creation of update-processing resources.
    virtual void processing_started() = 0;
    /// Delivers one coalescible request to an available processor.
    virtual void trigger_processing() = 0;
};

/// Implements UPD-006 without making task-creation failure permanently fatal.
class UpdateTaskInitialization {
public:
    explicit UpdateTaskInitialization(UpdateTaskInitializationPort& port);

    /// Makes the direct boot attempt and then submits the required boot request.
    void boot();

    /// Retries unavailable processing before delivering or dropping the request.
    void request();

private:
    bool try_initialize();

    UpdateTaskInitializationPort& port_;
    std::atomic_bool available_{false};
    std::atomic_bool initializing_{false};
};

}  // namespace firmware::application
