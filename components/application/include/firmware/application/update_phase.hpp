// Declares firmware-update recovery, phase publication, and visible progress.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {

// Holds the phase and percentage exposed through aggregate machine status.
struct UpdateStatus {
    std::uint8_t phase;
    std::uint8_t progress;

    // Supports exact status comparisons in callers and contract tests.
    bool operator==(const UpdateStatus& other) const {
        return phase == other.phase && progress == other.progress;
    }
};

// Isolates phase policy from persistent storage and host broadcasting.
class UpdatePhasePort {
public:
    // Enables safe destruction through a substituted phase adapter.
    virtual ~UpdatePhasePort() = default;

    // Persists one newest update phase in the update namespace.
    virtual bool persist_phase(std::uint8_t phase) = 0;

    // Broadcasts one update error to all host connections.
    virtual void broadcast(std::uint8_t type, std::string_view payload) = 0;
};

// Owns volatile status, four pending publications, and boot reconciliation.
class UpdatePhaseService {
public:
    // Creates an idle update state using the supplied persistence adapter.
    explicit UpdatePhaseService(UpdatePhasePort& port);

    // Applies the boot action selected by one persisted phase value.
    void reconcile_boot(std::uint8_t persisted_phase,
                        std::uint64_t now_milliseconds);

    // Clears a persisted prior-failure phase when an aggregate is opened.
    void aggregate_opened();

    // Changes volatile phase and nonblockingly adds one persistence request.
    void publish(std::uint8_t phase);

    // Persists only the newest queued phase and drains all pending requests.
    void process_pending();

    // Returns the number of persistence publications currently queued.
    std::size_t pending_count() const;

    // Broadcasts the exact format error under the shared one-second limiter.
    void broadcast_validation_error(std::uint64_t now_milliseconds);

    // Applies the specified rounded controller-transfer percentage.
    void set_controller_progress(std::uint32_t index,
                                 std::uint32_t frame_count);

    // Publishes successful controller completion for exactly three seconds.
    void controller_completed(std::uint64_t now_milliseconds);

    // Ends the transient success display when its deadline is reached.
    void tick(std::uint64_t now_milliseconds);

    // Returns volatile status, or persisted failure while otherwise idle.
    UpdateStatus status() const;

private:
    // Sends one broadcast only when the shared limiter permits it.
    void broadcast_limited(std::string_view message,
                           std::uint64_t now_milliseconds);

    UpdatePhasePort& port_;
    std::array<std::uint8_t, 4U> pending_phases_{};
    std::size_t pending_count_ = 0U;
    std::uint8_t persisted_phase_ = 0U;
    UpdateStatus volatile_status_{0U, 0U};
    std::optional<std::uint64_t> last_error_broadcast_milliseconds_;
    std::optional<std::uint64_t> success_deadline_milliseconds_;
};

}  // namespace firmware::application
