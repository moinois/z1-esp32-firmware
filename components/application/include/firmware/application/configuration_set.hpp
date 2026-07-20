// Declares live and SD config-set policy behind replaceable persistence I/O.
#pragma once

#include "firmware/application/live_configuration.hpp"
#include "firmware/core/frame.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace firmware::application {

// Adds SD replacement operations and responses to live-view loading.
class ConfigurationSetPort : public LiveConfigurationPort {
public:
    // Enables safe destruction through a substituted port implementation.
    ~ConfigurationSetPort() override = default;

    // Reads the complete active configuration or reports source failure.
    virtual std::optional<std::string> read_active_text(
        std::string_view path) = 0;

    // Writes the complete temporary replacement and reports success.
    virtual bool write_temporary(std::string_view path,
                                 std::string_view content) = 0;

    // Attempts unlink of the active file; the policy intentionally ignores result.
    virtual bool unlink_active(std::string_view path) = 0;

    // Renames the completed temporary file over the active path.
    virtual bool rename_temporary(std::string_view source,
                                  std::string_view destination) = 0;

    // Removes the temporary file after rename failure.
    virtual void remove_temporary(std::string_view path) = 0;

    // Sends one response to the destination selected by the command adapter.
    virtual void send(core::Frame frame) = 0;
};

// Executes bounded live updates and temporary-file SD replacement.
class ConfigurationSet {
public:
    // Parses at most three tokens and applies one recognized source update.
    static void execute(core::BytesView argument, LiveConfiguration& live,
                        ConfigurationSetPort& port);
};

}  // namespace firmware::application
