// Declares persisted runtime query and first-boot clearing command policy.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::application {

// Classifies one persisted signed runtime value read.
enum class RuntimeValueResult {
    success,
    missing,
    failure,
};

// Holds one signed value and its exact persistence result.
struct RuntimeSignedRead {
    RuntimeValueResult result;
    std::int64_t value;
};

// Classifies first_boot erase success, absence, and failure.
enum class RuntimeEraseResult {
    success,
    missing,
    failure,
};

// Isolates runtime commands from capacity, persistence, UTC, and routing.
class RuntimeCommandPort {
public:
    // Enables safe destruction through a substituted runtime command adapter.
    virtual ~RuntimeCommandPort() = default;

    // Attempts admission to the shared runtime-operation capacity.
    virtual bool admit_operation(std::uint32_t wait_milliseconds) = 0;

    // Opens the exact persistent runtime namespace.
    virtual bool open_namespace(std::string_view name_space) = 0;

    // Reads the persisted signed first_boot value.
    virtual RuntimeSignedRead read_first_boot(std::string_view key) = 0;

    // Reads one persisted unsigned runtime counter.
    virtual std::optional<std::uint64_t> read_counter(
        std::string_view key) = 0;

    // Formats first_boot in minute-resolution UTC or reports failure.
    virtual std::optional<std::string> format_utc_minute(
        std::int64_t seconds) = 0;

    // Erases only first_boot under its exact namespace and key.
    virtual RuntimeEraseResult erase_first_boot(std::string_view name_space,
                                                std::string_view key) = 0;

    // Releases one admitted runtime-operation slot.
    virtual void complete_operation() = 0;

    // Sends one response through origin-aware runtime routing.
    virtual void send_response(std::uint8_t type,
                               std::string_view payload) = 0;
};

// Implements sys-time and clearftm against persisted, not volatile, counters.
class RuntimeCommandService {
public:
    // Binds runtime command rules to replaceable persistence and routing ports.
    explicit RuntimeCommandService(RuntimeCommandPort& port);

    // Validates and executes one complete sys-time command.
    void handle_system_time(std::string_view command);

    // Validates and executes one complete clearftm command.
    void handle_clear_first_boot(std::string_view command);

private:
    // Reports whether a command has only accepted trailing whitespace.
    bool valid_shape(std::string_view command,
                     std::string_view exact_prefix) const;

    // Sends one exact type-0x83 runtime response.
    void respond(std::string_view payload);

    RuntimeCommandPort& port_;
};

}  // namespace firmware::application
