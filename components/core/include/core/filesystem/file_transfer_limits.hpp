/** @file
 *  @brief Protocol limits shared by host file-transfer parsing and state.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace firmware::core::file_transfer_limits {

/// Maximum accepted encoded path length, excluding implementation terminators.
inline constexpr std::size_t maximum_path_size = 255U;
/// Number of lowercase hexadecimal characters in an MD5 sidecar value.
inline constexpr std::size_t md5_text_size = 32U;
/// Maximum application data carried by one file-transfer block.
inline constexpr std::size_t data_block_size = 8192U;
/// Idle interval after which an unfinished transfer is abandoned.
inline constexpr std::uint64_t inactivity_timeout_milliseconds = 9000U;

}  // namespace firmware::core::file_transfer_limits
