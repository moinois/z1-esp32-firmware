// Implements storage retention through POSIX VFS and a bounded FreeRTOS task.
#include "storage_retention_adapter.hpp"

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firmware/application/storage_retention_service.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <optional>
#include <string>
#include <vector>

namespace firmware::target {
namespace {

constexpr char tag[] = "RETENTION";
constexpr char videos_directory[] = "/sd/videos";

class PosixRetentionPort final : public firmware::application::StorageRetentionPort {
public:
    std::optional<firmware::application::StorageUsage> read_usage() override {
        std::uint64_t total_bytes = 0U;
        std::uint64_t free_bytes = 0U;
        if (esp_vfs_fat_info("/sd", &total_bytes, &free_bytes) != ESP_OK) {
            return std::nullopt;
        }
        return firmware::application::StorageUsage{total_bytes, free_bytes};
    }

    std::optional<std::vector<firmware::application::RetentionCandidate>>
    list_video_directory() override {
        DIR* directory = opendir(videos_directory);
        if (directory == nullptr) return std::nullopt;
        std::vector<firmware::application::RetentionCandidate> entries;
        while (dirent* item = readdir(directory)) {
            if (item->d_name[0] == '.') continue;
            const std::string path = std::string(videos_directory) + "/" + item->d_name;
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
        ESP_LOGI(tag, "runtime counter persistence requested");
    }
};

void retention_task(void*) {
    PosixRetentionPort port;
    firmware::application::StorageRetentionService service;
    for (;;) {
        service.run_check(port);
        vTaskDelay(pdMS_TO_TICKS(60000U));
    }
}

}  // namespace

void StorageRetentionAdapter::start() {
    xTaskCreate(retention_task, "retention", 4096U, nullptr, 3U, nullptr);
}

}  // namespace firmware::target
