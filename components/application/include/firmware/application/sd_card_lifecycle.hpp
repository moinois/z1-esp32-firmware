// Declares deterministic SD-card monitoring behind replaceable target operations.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {

// Holds immutable FAT mount policy passed to the target adapter.
struct SdMountConfig {
    std::string_view mount_path;
    bool format_if_mount_fails;
    std::size_t maximum_open_files;
    std::size_t allocation_unit_size;
};

// Holds successfully queried volume capacity in whole mebibytes.
struct SdCapacity {
    std::uint64_t total_mib;
    std::uint64_t free_mib;
};

// Isolates card policy from GPIO, FAT, SDMMC, and diagnostic logging APIs.
class SdCardPort {
public:
    // Enables safe destruction through a substituted port implementation.
    virtual ~SdCardPort() = default;

    // Samples the active-low card-detect input as logical insertion.
    virtual bool card_inserted() = 0;

    // Attempts one mount with the complete immutable mount policy.
    virtual bool mount(const SdMountConfig& config) = 0;

    // Starts a new SD diagnostic log session after successful mounting.
    virtual void start_logging() = 0;

    // Stops and drains diagnostic logging before an unmount attempt.
    virtual void stop_and_drain_logging() = 0;

    // Attempts one immediate unmount and reports whether it succeeded.
    virtual bool unmount() = 0;

    // Returns total volume bytes or reports query failure.
    virtual std::optional<std::uint64_t> total_bytes() = 0;

    // Returns free volume bytes or reports query failure.
    virtual std::optional<std::uint64_t> free_bytes() = 0;
};

// Owns startup mounting, sampled debounce, transitions, and mounted state.
class SdCardLifecycle {
public:
    // Samples once, attempts the optional boot mount, and starts monitoring.
    void start(std::uint64_t now_milliseconds, SdCardPort& port);

    // Performs due monitoring work without blocking between samples.
    void poll(std::uint64_t now_milliseconds, SdCardPort& port);

    // Reports the last successfully established mount state.
    bool mounted() const;

    // Queries both capacities and truncates them to whole mebibytes.
    static std::optional<SdCapacity> read_capacity(SdCardPort& port);

private:
    void attempt_mount(SdCardPort& port);
    void accept_removal(SdCardPort& port);

    std::uint64_t next_sample_milliseconds_ = 0U;
    std::uint64_t pending_mount_milliseconds_ = 0U;
    std::uint8_t different_sample_count_ = 0U;
    bool settled_inserted_ = false;
    bool mount_pending_ = false;
    bool mounted_ = false;
    bool started_ = false;
};

}  // namespace firmware::application
