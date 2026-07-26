// Declares the shared recording-request state between command and media tasks.
#pragma once

namespace firmware::target {

// Stores one process-wide recording request without transport dependencies.
class RecordingRequestState {
public:
    // Sets the requested recording state from M951/M952 handling.
    void set_requested(bool requested);

    // Reads the current recording request for the recording task.
    bool requested() const;
};

}  // namespace firmware::target
