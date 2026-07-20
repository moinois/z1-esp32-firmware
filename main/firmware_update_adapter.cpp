// Implements SD aggregate loading and application update service composition.
#include "firmware_update_adapter.hpp"

#include "controller_command_loop.hpp"
#include "esp_image_validator.hpp"
#include "ota_update_adapter.hpp"
#include "nvs_key_value_adapter.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firmware/application/update_application.hpp"
#include "firmware/application/update_validation.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <atomic>

namespace firmware::target {
namespace {

constexpr char tag[] = "UPDATE";
std::atomic_bool update_requested{false};

class UpdateTargetPort final
    : public firmware::application::UpdateValidationPort {
public:
    explicit UpdateTargetPort(OtaUpdateAdapter& ota) : ota_(ota) {}

    void remove_partial(std::string_view path) override {
        static_cast<void>(std::remove(std::string(path).c_str()));
    }
    void clear_attributes(std::string_view) override {}
    firmware::application::UpdateLoadResult load_aggregate(
        std::string_view path) override {
        std::FILE* file = std::fopen(std::string(path).c_str(), "rb");
        if (file == nullptr) {
            return {firmware::application::UpdateLoadFailure::absent, {}};
        }
        if (std::fseek(file, 0L, SEEK_END) != 0) {
            std::fclose(file);
            return {firmware::application::UpdateLoadFailure::seek, {}};
        }
        const long length = std::ftell(file);
        if (length < 0L || std::fseek(file, 0L, SEEK_SET) != 0) {
            std::fclose(file);
            return {firmware::application::UpdateLoadFailure::size, {}};
        }
        firmware::core::ByteVector bytes(static_cast<std::size_t>(length));
        const std::size_t count = std::fread(bytes.data(), 1U, bytes.size(), file);
        std::fclose(file);
        if (count != bytes.size()) {
            return {firmware::application::UpdateLoadFailure::short_read, {}};
        }
        return {firmware::application::UpdateLoadFailure::none, std::move(bytes)};
    }
    void aggregate_opened() override {}
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

private:
    OtaUpdateAdapter& ota_;
    EspImageValidator validator_;
};

class UpdateApplicationTargetPort final
    : public firmware::application::UpdateApplicationPort {
public:
    explicit UpdateApplicationTargetPort(OtaUpdateAdapter& ota) : ota_(ota) {}
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
    void remove_aggregate(std::string_view path) override { ota_.remove_aggregate(path); }
    void send_controller_reset() override { ota_.send_controller_reset(); }
    void restart_mainboard() override { ota_.restart_mainboard(); }

private:
    OtaUpdateAdapter& ota_;
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
        ESP_LOGI(tag, "recovered OTA phase %u",
                 static_cast<unsigned>(*persisted_phase));
        if (*persisted_phase == 4U) {
            static_cast<void>(NvsKeyValueAdapter{}.write_u8("ota_state", "phase", 0U));
        }
    }
    update_requested.store(true);
    for (;;) {
        if (update_requested.exchange(false)) {
            process_update_once();
        }
        vTaskDelay(pdMS_TO_TICKS(250U));
    }
}

}  // namespace

void FirmwareUpdateAdapter::start() {
    if (xTaskCreate(update_task, "firmware_update", 8192U, nullptr, 4U, nullptr) !=
        pdPASS) {
        ESP_LOGW(tag, "could not create firmware update task");
    }
}

void request_firmware_update_processing() {
    update_requested.store(true);
}

}  // namespace firmware::target
