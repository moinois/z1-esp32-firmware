/** @file @brief Implements exact DIAG-028 POSIX filesystem formatting. */
#include "application/diagnostics/filesystem_diagnostics.hpp"

namespace firmware::application {

std::string filesystem_opendir_failure(std::string_view path, int error) {
    return "opendir " + std::string(path) + " failed: errno=" +
           std::to_string(error);
}

std::string filesystem_mkdir_failure(std::string_view path, int error) {
    return "mkdir failed: " + std::string(path) + " errno=" +
           std::to_string(error);
}

std::string filesystem_remove_failure(std::string_view path, int result,
                                      int error) {
    return "rm failed: " + std::string(path) + " result=" +
           std::to_string(result) + " errno=" + std::to_string(error);
}

std::string filesystem_rename_failure(std::string_view source,
                                      std::string_view destination, int error) {
    return "rename " + std::string(source) + " -> " +
           std::string(destination) + " failed: errno=" +
           std::to_string(error);
}

}  // namespace firmware::application
