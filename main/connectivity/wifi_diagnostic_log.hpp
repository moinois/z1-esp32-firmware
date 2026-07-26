// Declares the persistent, bounded Wi-Fi provisioning diagnostic log.
#pragma once

#include <string>
#include <string_view>

namespace firmware::target {

// Stores recent Wi-Fi provisioning stages without retaining credentials.
class WifiDiagnosticLog {
public:
    // Appends one bounded diagnostic line and retains the newest entries.
    bool append(std::string_view message) const;

    // Emits one diagnostic line to COM and retains it persistently.
    bool trace(std::string_view message) const;

    // Returns all retained diagnostic lines in chronological order.
    std::string read() const;

    // Removes all retained diagnostic lines.
    bool clear() const;
};

// Returns the process-wide persistent Wi-Fi diagnostic log.
WifiDiagnosticLog& wifi_diagnostic_log();

}  // namespace firmware::target
