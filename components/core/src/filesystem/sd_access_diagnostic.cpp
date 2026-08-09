/** @file @brief Implements stable human-readable SD access failure classification. */
#include "core/filesystem/sd_access_diagnostic.hpp"

#include <cerrno>

namespace firmware::core {

std::string_view sd_access_failure_reason(bool mounted, int error_number) {
    if (!mounted) return "SD card not mounted";
    switch (error_number) {
        case ENOENT: return "not found";
        case EACCES:
        case EPERM: return "permission denied";
        case ENOTDIR: return "not a directory";
        case EIO: return "I/O error";
        default: return "POSIX error";
    }
}

}  // namespace firmware::core
