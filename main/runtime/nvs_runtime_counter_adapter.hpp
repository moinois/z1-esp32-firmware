/** @file @brief Declares the ESP-IDF NVS backend for runtime counters. */
#pragma once

#include "application/runtime/runtime_counters.hpp"
#include "nvs_key_value_adapter.hpp"

namespace firmware::target {

/** Persists first-boot and runtime counters in their specified NVS namespace. */
class NvsRuntimeCounterAdapter final : public firmware::application::RuntimeCounterPort {
public:
    /// Reads first-boot and runtime counters from the normative NVS keys.
    firmware::application::FirstBootRead read_first_boot(
        std::string_view name_space, std::string_view key) override;
    std::optional<std::uint64_t> read_counter(
        std::string_view name_space, std::string_view key) override;
    bool write_first_boot(std::string_view name_space, std::string_view key,
                          std::int64_t seconds) override;
    bool write_counter(std::string_view name_space, std::string_view key,
                       std::uint64_t value) override;

private:
    NvsKeyValueAdapter nvs_;
};

}  // namespace firmware::target
