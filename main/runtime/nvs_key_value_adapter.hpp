/** @file @brief Declares a small ESP-IDF NVS adapter for persistent scalar values. */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include "esp_err.h"

namespace firmware::target {

/** Distinguishes absent data from driver/storage failure at NVS boundaries. */
enum class NvsReadState { found, missing, failure };

/** String read outcome that preserves missing-versus-failure semantics. */
struct NvsStringRead {
    NvsReadState state = NvsReadState::failure;
    std::string value;
};

/** Unsigned scalar read outcome that preserves missing-versus-failure semantics. */
struct NvsU64Read {
    NvsReadState state = NvsReadState::failure;
    std::uint64_t value = 0U;
    bool open_failed = false;
    esp_err_t error = ESP_OK;
};

enum class NvsMutationStage { none, open, mutation, commit };
struct NvsMutationResult {
    NvsMutationStage stage = NvsMutationStage::none;
    esp_err_t error = ESP_OK;
    bool succeeded() const { return stage == NvsMutationStage::none; }
};

/// Reads and writes NVS values while keeping namespace/key handling centralized.
class NvsKeyValueAdapter {
public:
    NvsStringRead read_string(std::string_view name_space,
                              std::string_view key) const;
    std::optional<std::uint64_t> read_u64(std::string_view name_space,
                                          std::string_view key) const;
    NvsU64Read read_u64_state(std::string_view name_space,
                              std::string_view key) const;
    std::optional<std::uint8_t> read_u8(std::string_view name_space,
                                        std::string_view key) const;
    std::optional<std::int64_t> read_i64(std::string_view name_space,
                                         std::string_view key) const;
    bool write_string(std::string_view name_space, std::string_view key,
                      std::string_view value) const;
    bool write_u64(std::string_view name_space, std::string_view key,
                   std::uint64_t value) const;
    NvsMutationResult write_u64_detailed(std::string_view name_space,
                                         std::string_view key,
                                         std::uint64_t value) const;
    bool write_u8(std::string_view name_space, std::string_view key,
                  std::uint8_t value) const;
    NvsMutationResult write_u8_detailed(std::string_view name_space,
                                        std::string_view key,
                                        std::uint8_t value) const;
    bool write_i64(std::string_view name_space, std::string_view key,
                   std::int64_t value) const;
    NvsMutationResult write_i64_detailed(std::string_view name_space,
                                         std::string_view key,
                                         std::int64_t value) const;
    /// Erases one key and distinguishes missing keys from storage failures.
    NvsReadState erase_key(std::string_view name_space,
                           std::string_view key) const;
};

}  // namespace firmware::target
