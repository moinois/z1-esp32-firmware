// Declares aggregate update boot and local-command trigger coalescing.
#pragma once

#include <string_view>

namespace firmware::application {

// Isolates update triggering from filesystem and persisted-state recovery.
class UpdateTriggerPort {
public:
    // Enables safe destruction through a substituted trigger adapter.
    virtual ~UpdateTriggerPort() = default;

    // Best-effort removes the partial aggregate from its exact path.
    virtual void remove_partial(std::string_view path) = 0;

    // Reconciles the persisted update phase during boot.
    virtual void reconcile_persisted_state() = 0;
};

// Coalesces boot and command requests into one pending aggregate operation.
class UpdateTriggerService {
public:
    // Creates an initially idle trigger using the supplied boot adapter.
    explicit UpdateTriggerService(UpdateTriggerPort& port);

    // Performs boot cleanup/recovery and requests aggregate processing.
    void boot();

    // Recognizes upgrade/reset prefixes and requests processing without reply.
    bool handle_command(std::string_view command);

    // Takes and clears the one coalesced pending processing request.
    bool take_request();

private:
    // Sets the single pending request regardless of its current value.
    void request_processing();

    UpdateTriggerPort& port_;
    bool request_pending_ = false;
};

}  // namespace firmware::application
