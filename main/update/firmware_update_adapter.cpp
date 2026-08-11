/** @file @brief Implements SD aggregate loading and application update service composition. */
#include "firmware_update_adapter.hpp"

#include "controller_command_loop.hpp"
#include "tcp_control_adapter.hpp"
#include "esp_image_validator.hpp"
#include "ota_update_adapter.hpp"
#include "nvs_key_value_adapter.hpp"
#include "runtime_status_adapter.hpp"
#include "update_phase_persistence.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "application/update/update_application.hpp"
#include "application/update/update_controller.hpp"
#include "application/update/update_deletion.hpp"
#include "application/update/update_task_initialization.hpp"
#include "application/update/update_validation.hpp"
#include "application/diagnostics/update_diagnostics.hpp"
#include "core/filesystem/sd_user_path.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <atomic>
#include <sys/stat.h>
#include <cerrno>

namespace firmware::target {
namespace {

constexpr char tag[] = "app_upgrade";
constexpr std::uint32_t update_monitor_interval_milliseconds = 250U;
constexpr std::uint32_t update_task_stack_size = 8192U;
constexpr UBaseType_t update_task_priority = 4U;
std::atomic_bool update_requested{false};
std::atomic<firmware::application::UpdateControllerMonitor*> controller_monitor{
    nullptr};

void update_task(void*);

class UpdateTaskTargetPort final
    : public firmware::application::UpdateTaskInitializationPort {
public:
    bool start_processing() override {
        return xTaskCreate(update_task, "firmware_update", update_task_stack_size,
                           nullptr, update_task_priority, nullptr) == pdPASS;
    }
    void warn_not_started() override {
        ESP_LOGW(tag, "[ota_task] not started yet, starting now");
    }
    void processing_started() override {
        ESP_LOGI(tag, "[ota_task] started (stack in internal DRAM)");
    }
    void processing_start_failed() override {
        ESP_LOGE(tag, "[ota_task] create failed");
    }
    void trigger_processing() override { update_requested.store(true); }
};

firmware::application::UpdateTaskInitialization& task_initialization() {
    static UpdateTaskTargetPort port;
    static firmware::application::UpdateTaskInitialization initialization(port);
    return initialization;
}

class UpdateControllerTargetPort final
    : public firmware::application::UpdateControllerPort {
public:
    bool staged_controller_exists() const override {
        struct stat information{};
        const std::string path =
            firmware::core::physical_sd_path("/lpc1768.bin");
        return stat(path.c_str(), &information) == 0 &&
               S_ISREG(information.st_mode);
    }
    bool firmware_transfer_active() const override {
        return controller_firmware_transfer_active();
    }
    bool configuration_transfer_active() const override {
        return controller_configuration_transfer_active();
    }
    bool factory_transfer_active() const override {
        return controller_factory_transfer_active();
    }
    void send_controller_reset() override {
        OtaUpdateAdapter{}.send_controller_reset();
    }
    void publish_error() override {
        publish_controller_transfer_status(3U, 0U);
    }
    void remove_staged_controller(std::string_view path) override {
        static_cast<void>(std::remove(std::string(path).c_str()));
    }
    void controller_completed(std::uint64_t) override {
        publish_controller_transfer_status(4U, 100U);
    }
};

class UpdateTargetPort final
    : public firmware::application::UpdateValidationPort {
public:
    explicit UpdateTargetPort(OtaUpdateAdapter& ota) : ota_(ota) {}

    void remove_partial(std::string_view path) override {
        static_cast<void>(std::remove(std::string(path).c_str()));
    }
    void clear_attributes(std::string_view path) override {
        static_cast<void>(chmod(std::string(path).c_str(), 0666));
    }
    firmware::application::UpdateLoadResult load_aggregate(
        std::string_view path) override {
        std::FILE* file = std::fopen(std::string(path).c_str(), "rb");
        if (file == nullptr) {
            return {firmware::application::UpdateLoadFailure::absent, {}};
        }
        static_cast<void>(std::fseek(file, 0L, SEEK_END));
        const std::uint32_t extent = static_cast<std::uint32_t>(std::ftell(file));
        static_cast<void>(std::fseek(file, 0L, SEEK_SET));
        auto bytes = firmware::application::UpdateBytes::allocate(extent);
        if (!bytes.has_value()) {
            std::fclose(file);
            ESP_LOGE(tag, "Failed to allocate memory");
            return {firmware::application::UpdateLoadFailure::allocation, {}};
        }
        const std::size_t count = std::fread(bytes->data(), 1U, bytes->size(), file);
        std::fclose(file);
        if (count != bytes->size()) {
            ESP_LOGE(tag, "Failed to read complete file");
            return {firmware::application::UpdateLoadFailure::short_read, {}};
        }
        return {firmware::application::UpdateLoadFailure::none, std::move(*bytes)};
    }
    void aggregate_opened() override {
        static_cast<void>(persist_update_phase(0U));
    }
    void remove_aggregate(std::string_view path) override {
        static_cast<void>(std::remove(std::string(path).c_str()));
    }
    void publish_error() override { ESP_LOGW(tag, "update validation failed"); }
    void broadcast_validation_error(std::uint64_t) override {
        ESP_LOGW(tag, "update validation error broadcast requested");
    }
    void send_controller_packet(std::uint8_t type,
                                firmware::core::BytesView payload) override {
        enqueue_controller_frame({type, {payload.begin(), payload.end()}});
    }
    bool valid_mainboard_image(firmware::core::BytesView image) override {
        return validator_.valid_mainboard_image(image);
    }
    void report_valid_header(const firmware::core::UpdateHeader& header,
                             firmware::core::BytesView encoded_header) override {
        for (const auto& line :
             firmware::application::aggregate_header_diagnostics(header,
                                                                  encoded_header)) {
            ESP_LOGI(tag, "%s", line.c_str());
        }
    }

private:
    OtaUpdateAdapter& ota_;
    EspImageValidator validator_;
};

// Maps target filesystem effects to the bounded update-deletion policy.
class UpdateDeletionTargetPort final
    : public firmware::application::UpdateDeletionPort {
public:
    firmware::application::UpdateDeleteResult unlink_file(
        std::string_view path) override {
        if (std::remove(std::string(path).c_str()) == 0 || errno == ENOENT) {
            unlink_error_ = 0;
            return firmware::application::UpdateDeleteResult::success;
        }
        unlink_error_ = errno;
        if (errno == EBUSY) {
            return firmware::application::UpdateDeleteResult::busy;
        }
        if (errno == EACCES || errno == EPERM) {
            return firmware::application::UpdateDeleteResult::permission_denied;
        }
        if (errno == EROFS) {
            return firmware::application::UpdateDeleteResult::read_only_filesystem;
        }
        return firmware::application::UpdateDeleteResult::other_failure;
    }

    bool clear_fat_attributes(std::string_view path) override {
        return chmod(std::string(path).c_str(), 0666U) == 0;
    }

    bool set_mode(std::string_view path, std::uint32_t mode) override {
        const bool succeeded =
            chmod(std::string(path).c_str(), static_cast<mode_t>(mode)) == 0;
        mode_error_ = succeeded ? 0 : errno;
        return succeeded;
    }

    void report_mode_failure(std::string_view path) override {
        const auto message = firmware::application::update_delete_mode_failure(
            path, mode_error_, std::strerror(mode_error_));
        ESP_LOGE(tag, "%s", message.c_str());
    }

    void report_unrecoverable(std::string_view path) override {
        const auto message = firmware::application::update_delete_unrecoverable(
            path, unlink_error_, std::strerror(unlink_error_));
        ESP_LOGE(tag, "%s", message.c_str());
    }

    void delay_milliseconds(std::uint32_t duration) override {
        vTaskDelay(pdMS_TO_TICKS(duration));
    }

    void broadcast(std::uint8_t type, std::string_view payload) override {
        broadcast_tcp_frame({type, {payload.begin(), payload.end()}});
    }

private:
    int unlink_error_ = 0;
    int mode_error_ = 0;
};

class UpdateApplicationTargetPort final
    : public firmware::application::UpdateApplicationPort {
public:
    explicit UpdateApplicationTargetPort(OtaUpdateAdapter& ota)
        : ota_(ota), deletion_(deletion_port_) {}
    void publish_phase(std::uint8_t phase) override { ota_.publish_phase(phase); }
    bool select_inactive_partition() override { return ota_.select_inactive_partition(); }
    bool begin_mainboard_write(std::uint32_t size) override {
        return ota_.begin_mainboard_write(size);
    }
    bool write_mainboard(firmware::core::BytesView image) override {
        return ota_.write_mainboard(image);
    }
    bool finalize_mainboard_write() override { return ota_.finalize_mainboard_write(); }
    bool select_mainboard_for_boot() override { return ota_.select_mainboard_for_boot(); }
    void abort_mainboard_write() override { ota_.abort_mainboard_write(); }
    void stage_controller(std::string_view path,
                          firmware::core::BytesView image) override {
        ota_.stage_controller(path, image);
    }
    void persist_phase_direct(std::uint8_t phase) override {
        ota_.persist_phase_direct(phase);
    }
    void remove_aggregate(std::string_view path) override {
        static_cast<void>(deletion_.remove(path));
    }
    void send_controller_reset() override { ota_.send_controller_reset(); }
    void restart_mainboard() override { ota_.restart_mainboard(); }

private:
    OtaUpdateAdapter& ota_;
    UpdateDeletionTargetPort deletion_port_;
    firmware::application::UpdateDeletionService deletion_;
};

void process_update_once() {
    OtaUpdateAdapter ota;
    UpdateTargetPort validation_port(ota);
    firmware::application::UpdateValidationService validation(validation_port);
    const auto package = validation.validate(0U);
    if (!package.has_value()) {
        return;
    }
    UpdateApplicationTargetPort application_port(ota);
    firmware::application::UpdateApplicationService application(application_port);
    static_cast<void>(application.apply(*package));
}

void update_task(void*) {
    vTaskDelay(pdMS_TO_TICKS(1000U));
    const auto persisted_phase = NvsKeyValueAdapter{}.read_u8("ota_state", "phase");
    if (persisted_phase.has_value()) {
        struct stat staged_information{};
        const std::string staged_path =
            firmware::core::physical_sd_path("/lpc1768.bin");
        const bool staged_exists =
            stat(staged_path.c_str(), &staged_information) == 0 &&
            S_ISREG(staged_information.st_mode);
        if (const auto diagnostic =
                firmware::application::update_recovery_diagnostic(
                    *persisted_phase, staged_exists);
            diagnostic.has_value()) {
            if (diagnostic->warning) ESP_LOGW(tag, "%s", diagnostic->message.c_str());
            else ESP_LOGI(tag, "%s", diagnostic->message.c_str());
        }
        if (*persisted_phase == 4U) {
            static_cast<void>(persist_update_phase(0U));
        }
    }
    UpdateControllerTargetPort controller_port;
    firmware::application::UpdateControllerMonitor monitor(controller_port);
    controller_monitor.store(&monitor, std::memory_order_release);
    monitor.start(xTaskGetTickCount() * portTICK_PERIOD_MS);
    for (;;) {
        monitor.tick(xTaskGetTickCount() * portTICK_PERIOD_MS);
        if (update_requested.exchange(false)) {
            ESP_LOGI(tag, "[ota_task] OTA trigger received");
            process_update_once();
        }
        vTaskDelay(pdMS_TO_TICKS(update_monitor_interval_milliseconds));
    }
}

}  // namespace

void FirmwareUpdateAdapter::start() {
    task_initialization().boot();
}

void request_firmware_update_processing() {
    task_initialization().request();
}

void notify_controller_transfer_completed(std::uint64_t now_milliseconds) {
    auto* monitor = controller_monitor.load(std::memory_order_acquire);
    if (monitor != nullptr) monitor->controller_completed(now_milliseconds);
}

void notify_controller_transfer_failed() {
    auto* monitor = controller_monitor.load(std::memory_order_acquire);
    if (monitor != nullptr) monitor->transfer_failed();
}

void notify_controller_transfer_cancelled() {
    auto* monitor = controller_monitor.load(std::memory_order_acquire);
    if (monitor != nullptr) monitor->transfer_cancelled();
}

void notify_controller_transfer_timeout(bool qualifying) {
    auto* monitor = controller_monitor.load(std::memory_order_acquire);
    if (monitor != nullptr) monitor->transfer_timed_out(qualifying);
}

}  // namespace firmware::target
