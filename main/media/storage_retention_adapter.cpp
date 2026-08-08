/** @file @brief Implements storage retention through POSIX VFS and a bounded FreeRTOS task. */
#include "storage_retention_adapter.hpp"

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firmware/application/storage_retention_service.hpp"
#include "firmware/core/sd_user_path.hpp"
#include "runtime_counter_task.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <optional>
#include <cstdint>
#include <string>
#include <vector>

namespace firmware::target {
namespace {

constexpr char tag[] = "RETENTION";
const std::string videos_directory =
    firmware::core::physical_sd_path("/videos");
constexpr std::uint32_t retention_interval_milliseconds = 60000U;
constexpr std::uint32_t retention_task_stack_size = 4096U;
constexpr UBaseType_t retention_task_priority = 3U;

class PosixRetentionPort final : public firmware::application::StorageRetentionPort {
public:
    std::optional<firmware::application::StorageUsage> read_usage() override {
        std::uint64_t total_bytes = 0U;
        std::uint64_t free_bytes = 0U;
        if (esp_vfs_fat_info(firmware::core::sd_mount_path.data(), &total_bytes,
                             &free_bytes) != ESP_OK) {
            return std::nullopt;
        }
        return firmware::application::StorageUsage{total_bytes, free_bytes};
    }

    std::optional<std::vector<firmware::application::RetentionCandidate>>
    list_video_directory() override {
        DIR* directory = opendir(videos_directory.c_str());
        if (directory == nullptr) return std::nullopt;
        std::vector<firmware::application::RetentionCandidate> entries;
        while (dirent* item = readdir(directory)) {
            if (item->d_name[0] == '.') continue;
            const std::string path = videos_directory + "/" + item->d_name;
            struct stat information {};
            if (stat(path.c_str(), &information) != 0) continue;
            entries.push_back({path, static_cast<std::uint64_t>(information.st_mtime),
                               S_ISREG(information.st_mode)});
        }
        closedir(directory);
        return entries;
    }

    bool remove_file(std::string_view path) override {
        return unlink(std::string(path).c_str()) == 0;
    }

    void request_persistence() override {
        request_runtime_persistence(
            static_cast<std::uint64_t>(xTaskGetTickCount() * portTICK_PERIOD_MS));
    }
};

void retention_task(void*) {
    PosixRetentionPort port;
    firmware::application::StorageRetentionService service;
    for (;;) {
        service.run_check(port);
        vTaskDelay(pdMS_TO_TICKS(retention_interval_milliseconds));
    }
}

}  // namespace

void StorageRetentionAdapter::start() {
    xTaskCreate(retention_task, "retention", retention_task_stack_size, nullptr,
                retention_task_priority, nullptr);
}

}  // namespace firmware::target
