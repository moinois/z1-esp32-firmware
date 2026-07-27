// Implements bounded update deletion retries and exact recovery delays.
#include "firmware/application/update_deletion.hpp"
#include "firmware/core/sd_user_path.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_unlink_attempts = 3U;
constexpr std::uint32_t writable_file_mode = 0666U;
constexpr std::uint32_t busy_delay_milliseconds = 500U;
constexpr std::uint32_t permission_delay_milliseconds = 100U;
constexpr std::uint32_t retry_gap_milliseconds = 200U;
constexpr std::uint8_t update_broadcast_type = 0x90U;
constexpr std::string_view delete_error_prefix = "Error: failed to delete [";
constexpr std::string_view delete_error_suffix =
    "], please delete manually.\r\n";

}  // namespace

UpdateDeletionService::UpdateDeletionService(UpdateDeletionPort& port)
    : port_(port) {}

bool UpdateDeletionService::remove(std::string_view path) {
    for (std::size_t attempt = 0U; attempt < maximum_unlink_attempts;
         ++attempt) {
        const UpdateDeleteResult result = port_.unlink_file(path);
        if (result == UpdateDeleteResult::success) {
            return true;
        }

        bool recoverable = false;
        if (result == UpdateDeleteResult::busy) {
            port_.delay_milliseconds(busy_delay_milliseconds);
            recoverable = true;
        } else if (result == UpdateDeleteResult::permission_denied ||
                   result == UpdateDeleteResult::read_only_filesystem) {
            recoverable = recover_permissions(path, result);
        }
        if (!recoverable) {
            broadcast_failure(path);
            return false;
        }
        if (attempt + 1U < maximum_unlink_attempts) {
            port_.delay_milliseconds(retry_gap_milliseconds);
        }
    }

    broadcast_failure(path);
    return false;
}

bool UpdateDeletionService::recover_permissions(
    std::string_view path, UpdateDeleteResult failure) {
    bool adjusted = port_.clear_fat_attributes(path);
    if (!adjusted && failure == UpdateDeleteResult::permission_denied) {
        adjusted = port_.set_mode(path, writable_file_mode);
    }
    if (adjusted) {
        port_.delay_milliseconds(permission_delay_milliseconds);
    }
    return adjusted;
}

void UpdateDeletionService::broadcast_failure(std::string_view path) {
    const std::string displayed_path = core::logical_sd_path(path);
    std::string message;
    message.reserve(delete_error_prefix.size() + displayed_path.size() +
                    delete_error_suffix.size());
    message.append(delete_error_prefix);
    message.append(displayed_path);
    message.append(delete_error_suffix);
    port_.broadcast(update_broadcast_type, message);
}

}  // namespace firmware::application
