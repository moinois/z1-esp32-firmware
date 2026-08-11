/** @file @brief Declares one-shot lazy initialization for live media resources. */
#pragma once

#include <atomic>

namespace firmware::application {

/// Isolates target camera and media-resource creation from request policy.
class LiveInitializationPort {
public:
    virtual ~LiveInitializationPort() = default;
    /// Creates every capability shared by live streaming and preview open.
    virtual bool initialize_live_media() = 0;
};

/// Attempts live-media initialization once per boot and retains its outcome.
class LiveInitialization {
public:
    explicit LiveInitialization(LiveInitializationPort& port);

    /// Returns the retained availability, performing the sole attempt if needed.
    bool ensure_available();

private:
    enum class State : unsigned char {
        not_attempted,
        initializing,
        available,
        unavailable,
    };

    LiveInitializationPort& port_;
    std::atomic<State> state_{State::not_attempted};
};

}  // namespace firmware::application
