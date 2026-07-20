// Declares persistent first-boot, power-on, and streamed-play time accounting.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {

// Classifies first-boot key reads without conflating absence and failure.
enum class FirstBootReadResult {
    present,
    missing,
    failure,
};

// Holds one signed first-boot read and its availability state.
struct FirstBootRead {
    FirstBootReadResult result;
    std::int64_t seconds;
};

// Isolates runtime accounting from persistent namespace implementation.
class RuntimeCounterPort {
public:
    // Enables safe destruction through a substituted runtime adapter.
    virtual ~RuntimeCounterPort() = default;

    // Reads the signed first-boot value from the exact namespace and key.
    virtual FirstBootRead read_first_boot(std::string_view name_space,
                                          std::string_view key) = 0;

    // Attempts to persist the signed first-boot value.
    virtual bool write_first_boot(std::string_view name_space,
                                  std::string_view key,
                                  std::int64_t seconds) = 0;

    // Reads one unsigned runtime counter, or reports missing/unreadable data.
    virtual std::optional<std::uint64_t> read_counter(
        std::string_view name_space, std::string_view key) = 0;

    // Attempts one silent unsigned runtime counter update.
    virtual bool write_counter(std::string_view name_space,
                               std::string_view key,
                               std::uint64_t value) = 0;
};

// Owns in-memory counter baselines and discards fractional seconds per save.
class RuntimeCounterService {
public:
    // Creates zeroed counters before their persisted initialization.
    explicit RuntimeCounterService(RuntimeCounterPort& port);

    // Loads persisted counters and starts the power-on monotonic interval.
    void initialize(std::uint64_t monotonic_milliseconds);

    // Writes current Unix seconds only when first_boot is absent.
    void record_first_boot(std::int64_t unix_seconds);

    // Accumulates and silently attempts to save whole power-on seconds.
    void save_power_on(std::uint64_t monotonic_milliseconds);

    // Processes every play-running notification and both affected counters.
    void play_running_changed(bool running,
                              std::uint64_t monotonic_milliseconds);

    // Returns the current in-memory power-on total.
    std::uint64_t power_on_seconds() const;

    // Returns the current in-memory streamed-play machine total.
    std::uint64_t machine_seconds() const;

private:
    RuntimeCounterPort& port_;
    std::uint64_t power_on_seconds_ = 0U;
    std::uint64_t machine_seconds_ = 0U;
    std::uint64_t power_on_baseline_milliseconds_ = 0U;
    std::uint64_t play_started_milliseconds_ = 0U;
    bool play_running_ = false;
};

}  // namespace firmware::application
