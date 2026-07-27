// Defines limits shared by host file-transfer parsing and state machines.
#pragma once

#include <cstddef>
#include <cstdint>

namespace firmware::core::file_transfer_limits {

inline constexpr std::size_t maximum_path_size = 255U;
inline constexpr std::size_t md5_text_size = 32U;
inline constexpr std::size_t data_block_size = 8192U;
inline constexpr std::uint64_t inactivity_timeout_milliseconds = 9000U;

}  // namespace firmware::core::file_transfer_limits
