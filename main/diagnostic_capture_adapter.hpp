// Declares the ESP-IDF log-hook adapter for bounded diagnostic capture.
#pragma once

namespace firmware::target {

class DiagnosticCaptureAdapter {
public:
    // Installs the capture hook while preserving the existing console output.
    void start();
};

}  // namespace firmware::target
