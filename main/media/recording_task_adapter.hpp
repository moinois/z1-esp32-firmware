/** @file @brief Declares the FreeRTOS task that composes camera capture and AVI recording. */
#pragma once

namespace firmware::target {

/// Starts the nonfatal recording worker over shared request and play state.
class RecordingTaskAdapter {
public:
    /// Creates the bounded one-second recording task.
    void start();
};

}  // namespace firmware::target
