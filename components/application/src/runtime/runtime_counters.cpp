/** @file @brief Implements whole-second runtime accounting and silent persistence failures. */
#include "application/runtime/runtime_counters.hpp"
#include "application/runtime/runtime_persistence.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::uint64_t milliseconds_per_second = 1000U;

// Returns whole elapsed seconds and treats a backwards clock as no elapsed time.
std::uint64_t whole_elapsed_seconds(std::uint64_t start,
                                    std::uint64_t end) {
    if (end < start) {
        return 0U;
    }
    return (end - start) / milliseconds_per_second;
}

}  // namespace

RuntimeCounterService::RuntimeCounterService(RuntimeCounterPort& port)
    : port_(port) {}

void RuntimeCounterService::initialize(
    std::uint64_t monotonic_milliseconds) {
    power_on_seconds_ =
        port_.read_counter(runtime_persistence::name_space,
                           runtime_persistence::power_on_seconds_key).value_or(0U);
    machine_seconds_ =
        port_.read_counter(runtime_persistence::name_space,
                           runtime_persistence::machine_seconds_key).value_or(0U);
    power_on_baseline_milliseconds_ = monotonic_milliseconds;
    play_running_ = false;
}

void RuntimeCounterService::record_first_boot(std::int64_t unix_seconds) {
    const FirstBootRead read =
        port_.read_first_boot(runtime_persistence::name_space,
                              runtime_persistence::first_boot_key);
    if (read.result == FirstBootReadResult::missing) {
        static_cast<void>(port_.write_first_boot(
            runtime_persistence::name_space,
            runtime_persistence::first_boot_key, unix_seconds));
    }
}

void RuntimeCounterService::save_power_on(
    std::uint64_t monotonic_milliseconds) {
    power_on_seconds_ += whole_elapsed_seconds(
        power_on_baseline_milliseconds_, monotonic_milliseconds);
    power_on_baseline_milliseconds_ = monotonic_milliseconds;
    static_cast<void>(port_.write_counter(
        runtime_persistence::name_space,
        runtime_persistence::power_on_seconds_key, power_on_seconds_));
}

void RuntimeCounterService::play_running_changed(
    bool running, std::uint64_t monotonic_milliseconds) {
    save_power_on(monotonic_milliseconds);
    if (running && !play_running_) {
        play_running_ = true;
        play_started_milliseconds_ = monotonic_milliseconds;
        return;
    }
    if (!running && play_running_) {
        machine_seconds_ += whole_elapsed_seconds(
            play_started_milliseconds_, monotonic_milliseconds);
        play_running_ = false;
        static_cast<void>(port_.write_counter(
            runtime_persistence::name_space,
            runtime_persistence::machine_seconds_key, machine_seconds_));
    }
}

std::uint64_t RuntimeCounterService::power_on_seconds() const {
    return power_on_seconds_;
}

std::uint64_t RuntimeCounterService::machine_seconds() const {
    return machine_seconds_;
}

}  // namespace firmware::application
