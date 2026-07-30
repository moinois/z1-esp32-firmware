// Implements a volatile FAT block device in PSRAM for target-level SD tests.
#include "mock_sd_card_adapter.hpp"

#include "diskio_impl.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "sdkconfig.h"

#include "firmware/core/sd_user_path.hpp"
#include "sd_access_diagnostics.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#ifndef CONFIG_Z1_MOCK_SD_CAPACITY_KIB
#define CONFIG_Z1_MOCK_SD_CAPACITY_KIB 512
#endif

#ifndef CONFIG_Z1_MOCK_SD_MIN_FREE_PSRAM_KIB
#define CONFIG_Z1_MOCK_SD_MIN_FREE_PSRAM_KIB 512
#endif

namespace firmware::target {
namespace {

constexpr char tag[] = "MOCK_SD";
constexpr std::size_t sector_size = 512U;
constexpr std::size_t format_work_buffer_size = 4096U;

struct RamDiskState {
    std::uint8_t* bytes = nullptr;
    std::size_t byte_count = 0U;
    BYTE drive = FF_DRV_NOT_USED;
    FATFS* filesystem = nullptr;
    bool mounted = false;
};

RamDiskState disk;

// Reports whether the single simulated block device has allocated storage.
DSTATUS ram_status(BYTE drive) {
    return drive == disk.drive && disk.bytes != nullptr ? 0 : STA_NOINIT;
}

// Initializes an already allocated volatile block device.
DSTATUS ram_initialize(BYTE drive) {
    return ram_status(drive);
}

// Copies complete sectors from the simulated disk into the FatFS buffer.
DRESULT ram_read(BYTE drive, BYTE* buffer, std::uint32_t sector,
                 unsigned count) {
    const std::size_t offset = static_cast<std::size_t>(sector) * sector_size;
    const std::size_t length = static_cast<std::size_t>(count) * sector_size;
    if (ram_status(drive) != 0 || buffer == nullptr || count == 0U ||
        offset > disk.byte_count || length > disk.byte_count - offset) {
        return RES_PARERR;
    }
    std::memcpy(buffer, disk.bytes + offset, length);
    return RES_OK;
}

// Copies complete sectors from FatFS into the simulated disk.
DRESULT ram_write(BYTE drive, const BYTE* buffer, std::uint32_t sector,
                  unsigned count) {
    const std::size_t offset = static_cast<std::size_t>(sector) * sector_size;
    const std::size_t length = static_cast<std::size_t>(count) * sector_size;
    if (ram_status(drive) != 0 || buffer == nullptr || count == 0U ||
        offset > disk.byte_count || length > disk.byte_count - offset) {
        return RES_PARERR;
    }
    std::memcpy(disk.bytes + offset, buffer, length);
    return RES_OK;
}

// Supplies the geometry and synchronization operations required by FatFS.
DRESULT ram_ioctl(BYTE drive, BYTE command, void* buffer) {
    if (ram_status(drive) != 0) {
        return RES_NOTRDY;
    }
    switch (command) {
        case CTRL_SYNC:
            return RES_OK;
        case GET_SECTOR_COUNT:
            if (buffer == nullptr) return RES_PARERR;
            *static_cast<LBA_t*>(buffer) = disk.byte_count / sector_size;
            return RES_OK;
        case GET_SECTOR_SIZE:
            if (buffer == nullptr) return RES_PARERR;
            *static_cast<WORD*>(buffer) = sector_size;
            return RES_OK;
        case GET_BLOCK_SIZE:
            if (buffer == nullptr) return RES_PARERR;
            *static_cast<DWORD*>(buffer) = 1U;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}

// Releases every registration and allocation made by a partial or full mount.
void release_disk() {
    if (disk.drive != FF_DRV_NOT_USED) {
        char drive_name[3] = {static_cast<char>('0' + disk.drive), ':', '\0'};
        static_cast<void>(f_mount(nullptr, drive_name, 0));
    }
    if (disk.filesystem != nullptr) {
        static_cast<void>(esp_vfs_fat_unregister_path(
            firmware::core::sd_mount_path.data()));
        disk.filesystem = nullptr;
    }
    if (disk.drive != FF_DRV_NOT_USED) {
        ff_diskio_unregister(disk.drive);
        disk.drive = FF_DRV_NOT_USED;
    }
    if (disk.bytes != nullptr) {
        heap_caps_free(disk.bytes);
        disk.bytes = nullptr;
    }
    disk.byte_count = 0U;
    disk.mounted = false;
    set_sd_storage_mounted(false);
}

// Allocates, formats, registers, and mounts a fresh FAT volume at /sd.
bool create_and_mount_disk() {
    if (disk.mounted) {
        return true;
    }
    const std::size_t requested =
        static_cast<std::size_t>(CONFIG_Z1_MOCK_SD_CAPACITY_KIB) * 1024U;
    const std::size_t reserve =
        static_cast<std::size_t>(CONFIG_Z1_MOCK_SD_MIN_FREE_PSRAM_KIB) * 1024U;
    const std::size_t capacity = requested - (requested % sector_size);
    const std::size_t free_psram =
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (capacity < 256U * 1024U || free_psram < capacity + reserve) {
        ESP_LOGE(tag, "Insufficient PSRAM: free=%u requested=%u reserve=%u",
                 static_cast<unsigned>(free_psram),
                 static_cast<unsigned>(capacity),
                 static_cast<unsigned>(reserve));
        return false;
    }

    disk.bytes = static_cast<std::uint8_t*>(heap_caps_calloc(
        1U, capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (disk.bytes == nullptr) {
        ESP_LOGE(tag, "Could not allocate mock SD storage");
        return false;
    }
    disk.byte_count = capacity;
    if (ff_diskio_get_drive(&disk.drive) != ESP_OK) {
        release_disk();
        return false;
    }
    const ff_diskio_impl_t implementation{
        .init = ram_initialize,
        .status = ram_status,
        .read = ram_read,
        .write = ram_write,
        .ioctl = ram_ioctl,
    };
    ff_diskio_register(disk.drive, &implementation);
    char drive_name[3] = {static_cast<char>('0' + disk.drive), ':', '\0'};
    const esp_vfs_fat_conf_t vfs_config{
        .base_path = firmware::core::sd_mount_path.data(),
        .fat_drive = drive_name,
        .max_files = 16U,
    };
    if (esp_vfs_fat_register_cfg(&vfs_config, &disk.filesystem) != ESP_OK) {
        release_disk();
        return false;
    }

    void* work_buffer = heap_caps_malloc(
        format_work_buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (work_buffer == nullptr) {
        release_disk();
        return false;
    }
    const MKFS_PARM format{FM_FAT | FM_SFD, 1U, 0U, 0U, sector_size};
    const FRESULT format_result = f_mkfs(
        drive_name, &format, work_buffer, format_work_buffer_size);
    heap_caps_free(work_buffer);
    if (format_result != FR_OK || f_mount(disk.filesystem, drive_name, 1) != FR_OK) {
        release_disk();
        return false;
    }
    disk.mounted = true;
    set_sd_storage_mounted(true);
    ESP_LOGW(tag, "TEST BUILD: mounted %u KiB volatile mock SD at /sd",
             static_cast<unsigned>(capacity / 1024U));
    return true;
}

}  // namespace

bool MockSdCardAdapter::mount_for_boot() {
    return create_and_mount_disk();
}

void MockSdCardAdapter::start() {
    ESP_LOGW(tag, "SD adapter mode=MOCK; contents are lost on reset");
}

}  // namespace firmware::target
