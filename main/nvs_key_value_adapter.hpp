// Declares a small ESP-IDF NVS adapter for persistent scalar values.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::target {

enum class NvsReadState { found, missing, failure };

struct NvsStringRead {
    NvsReadState state = NvsReadState::failure;
    std::string value;
};

struct NvsU64Read {
    NvsReadState state = NvsReadState::failure;
    std::uint64_t value = 0U;
};

// Reads and writes NVS values while keeping namespace/key handling centralized.
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
    bool write_u8(std::string_view name_space, std::string_view key,
                  std::uint8_t value) const;
    bool write_i64(std::string_view name_space, std::string_view key,
                   std::int64_t value) const;
    // Erases one key and distinguishes missing keys from storage failures.
    NvsReadState erase_key(std::string_view name_space,
                           std::string_view key) const;
};

}  // namespace firmware::target
