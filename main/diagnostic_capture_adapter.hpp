// Declares the ESP-IDF log-hook adapter for bounded diagnostic capture.
#pragma once

#include "firmware/core/bytes.hpp"

#include <optional>

namespace firmware::target {

class DiagnosticCaptureAdapter {
public:
    // Installs the capture hook while preserving the existing console output.
    void start();
};

// Removes the oldest captured record for consumption by the SD log writer.
std::optional<firmware::core::ByteVector> take_captured_diagnostic();

}  // namespace firmware::target
