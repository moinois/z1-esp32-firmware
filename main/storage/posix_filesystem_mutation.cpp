/** @file @brief Implements shared filesystem mutations and exact DIAG-028 logs. */
#include "posix_filesystem_mutation.hpp"

#include "application/diagnostics/filesystem_diagnostics.hpp"
#include "esp_log.h"

#include <cerrno>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace firmware::target {
namespace {

constexpr char diagnostic_tag[] = "APP_FILE";

void log_error(const std::string& message) {
    ESP_LOGE(diagnostic_tag, "%s", message.c_str());
}

void remove_tree(const std::string& path) {
    if (path.empty()) {
        ESP_LOGW(diagnostic_tag, "remove_directory_recursive: empty path");
        return;
    }
    struct stat status{};
    if (stat(path.c_str(), &status) != 0) return;
    if (!S_ISDIR(status.st_mode)) {
        const int result = unlink(path.c_str());
        if (result != 0) {
            const int error = errno;
            log_error(firmware::application::filesystem_remove_failure(
                path, result, error));
        }
        return;
    }

    DIR* directory = opendir(path.c_str());
    if (directory == nullptr) {
        log_error(firmware::application::filesystem_opendir_failure(path,
                                                                     errno));
    } else {
        while (const dirent* entry = readdir(directory)) {
            const std::string name(entry->d_name);
            if (name == "." || name == "..") continue;
            remove_tree(path + "/" + name);
        }
        closedir(directory);
    }
    const int result = rmdir(path.c_str());
    if (result != 0) {
        const int error = errno;
        log_error(firmware::application::filesystem_remove_failure(
            path, result, error));
    }
}

}  // namespace

bool create_posix_directory(std::string_view path, std::uint32_t mode) {
    const std::string value(path);
    if (mkdir(value.c_str(), static_cast<mode_t>(mode)) == 0 || errno == EEXIST) {
        return true;
    }
    log_error(firmware::application::filesystem_mkdir_failure(path, errno));
    return false;
}

void remove_posix_tree(std::string_view path) {
    remove_tree(std::string(path));
}

bool rename_posix_path(std::string_view source, std::string_view destination) {
    const std::string old_path(source);
    const std::string new_path(destination);
    if (rename(old_path.c_str(), new_path.c_str()) == 0) return true;
    log_error(firmware::application::filesystem_rename_failure(
        source, destination, errno));
    return false;
}

}  // namespace firmware::target
