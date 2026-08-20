/** @file @brief Implements bounded POSIX file reads and serialized controller UART replies. */
#include "controller_transfer_adapter.hpp"
#include "configuration_file_store.hpp"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "controller_channel_adapter.hpp"
#include "runtime_status_adapter.hpp"
#include "firmware_update_adapter.hpp"

#include "core/protocol/frame.hpp"
#include "esp_timer.h"

#include <cstdio>
#include <cstdlib>
#include <climits>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace firmware::target {
namespace {

constexpr char firmware_transfer_tag[] = "dfu_LPC1768";
constexpr char configuration_transfer_tag[] = "cfg_LPC1768";
constexpr char factory_transfer_tag[] = "factory_LPC1768";

}  // namespace

ControllerTransferAdapter::ControllerTransferAdapter(
    ControllerChannelAdapter& channel)
    : channel_(channel) {
    // DIAG-034--DIAG-036 require these lifecycle records in production. Keep
    // only their three tags at INFO when release builds lower the global log
    // level to WARN; unrelated informational strings remain optimized out.
    esp_log_level_set(firmware_transfer_tag, ESP_LOG_INFO);
    esp_log_level_set(configuration_transfer_tag, ESP_LOG_INFO);
    esp_log_level_set(factory_transfer_tag, ESP_LOG_INFO);
}

bool ControllerTransferAdapter::configuration_available() {
    return ConfigurationFileStore{}.exists();
}

std::optional<std::vector<firmware::core::ByteVector>>
ControllerTransferAdapter::read_configuration_chunks(std::size_t chunk_size) {
    if (chunk_size == 0U) return std::nullopt;
    const auto lines = ConfigurationFileStore{}.read_lines();
    std::string content;
    for (const auto& line : lines) {
        content += line;
        content.push_back('\n');
    }
    std::vector<firmware::core::ByteVector> chunks;
    for (std::size_t offset = 0U; offset < content.size(); offset += chunk_size) {
        const std::size_t count = std::min(chunk_size, content.size() - offset);
        chunks.emplace_back(content.begin() + offset,
                            content.begin() + offset + count);
    }
    return chunks;
}

bool ControllerTransferAdapter::file_exists(std::string_view path) {
    struct stat information{};
    return stat(std::string(path).c_str(), &information) == 0 &&
           S_ISREG(information.st_mode);
}

std::optional<std::uint64_t> ControllerTransferAdapter::file_size(
    std::string_view path) {
    struct stat information{};
    if (stat(std::string(path).c_str(), &information) != 0 ||
        !S_ISREG(information.st_mode) || information.st_size < 0) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(information.st_size);
}

void ControllerTransferAdapter::panic_on_zero_frame_size() {
    std::abort();
}

std::optional<firmware::core::ByteVector> ControllerTransferAdapter::read_file(
    std::string_view path, std::uint64_t offset, std::size_t maximum_size) {
    if (maximum_size == 0U || offset > static_cast<std::uint64_t>(LONG_MAX)) {
        return std::nullopt;
    }
    std::FILE* file = std::fopen(std::string(path).c_str(), "rb");
    if (file == nullptr) {
        return std::nullopt;
    }
    // The motion-board protocol deliberately continues after a failed seek.
    // This mirrors the original firmware: fread() then consumes bytes from
    // whatever position the C stream retained rather than rejecting the frame.
    static_cast<void>(std::fseek(file, static_cast<long>(offset), SEEK_SET));
    firmware::core::ByteVector data(maximum_size);
    const std::size_t count = std::fread(data.data(), 1U, maximum_size, file);
    const bool failed = std::ferror(file) != 0;
    std::fclose(file);
    if (failed) return std::nullopt;
    data.resize(count);
    return data;
}

std::optional<std::vector<firmware::core::ByteVector>>
ControllerTransferAdapter::read_chunks(std::string_view path,
                                        std::size_t chunk_size) {
    const auto size = file_size(path);
    if (!size.has_value() || chunk_size == 0U) return std::nullopt;
    std::vector<firmware::core::ByteVector> chunks;
    for (std::uint64_t offset = 0U; offset < *size;) {
        const auto chunk = read_file(path, offset, chunk_size);
        if (!chunk.has_value() || chunk->empty()) return std::nullopt;
        chunks.push_back(*chunk);
        offset += chunk->size();
    }
    return chunks;
}

bool ControllerTransferAdapter::remove_file(std::string_view path) {
    return unlink(std::string(path).c_str()) == 0;
}

bool ControllerTransferAdapter::response_data_memory_available(std::size_t bytes) {
    void* probe = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    if (probe == nullptr) return false;
    heap_caps_free(probe);
    return true;
}

bool ControllerTransferAdapter::send(firmware::core::Frame frame) {
    constexpr std::size_t encoded_overhead =
        firmware::core::protocol::common_frame_overhead;
    if (!response_data_memory_available(frame.payload.size() + encoded_overhead)) {
        const auto family = static_cast<firmware::application::ControllerTransferFamily>(
            ((frame.type >> 4U) - 0x0cU));
        diagnose(firmware::application::controller_transfer_diagnostic(
            family,
            firmware::application::ControllerTransferDiagnosticEvent::encoded_frame_allocation_failure));
        return false;
    }
    const auto encoded = firmware::core::encode_controller_frame(frame);
    if (encoded.empty()) return false;
    diagnose(firmware::application::controller_transfer_sent_diagnostic(
        frame.type, frame.payload.size()));
    for (auto& diagnostic :
         firmware::application::controller_transfer_layout_diagnostics(
             frame.type, frame.payload)) {
        diagnose(std::move(diagnostic));
    }
    const int written = channel_.write(encoded);
    if (written != static_cast<int>(encoded.size())) {
        ESP_LOGE("uart_task", "UART send failed");
        return false;
    }
    return true;
}

void ControllerTransferAdapter::diagnose(
    firmware::application::ControllerTransferDiagnostic diagnostic) {
    if (diagnostic.error) {
        ESP_LOGE(diagnostic.tag.data(), "%s", diagnostic.message.c_str());
    } else {
        // ESP_LOG_LEVEL retains this normative INFO call site even when the
        // release profile compiles ordinary ESP_LOGI sites out.
        ESP_LOG_LEVEL(ESP_LOG_INFO, diagnostic.tag.data(), "%s",
                      diagnostic.message.c_str());
    }
}

void ControllerTransferAdapter::publish(
    firmware::application::FirmwareTransferEvent event, std::uint32_t index,
    std::uint32_t frame_count) {
    constexpr std::uint8_t controller_phase = 2U;
    constexpr std::uint8_t error_phase = 3U;
    constexpr std::uint8_t success_phase = 4U;
    switch (event) {
        case firmware::application::FirmwareTransferEvent::started:
            publish_controller_transfer_status(controller_phase, 0U);
            break;
        case firmware::application::FirmwareTransferEvent::progress: {
            const std::uint32_t progress = frame_count == 0U
                ? 0U
                : (100U * index + frame_count / 2U) / frame_count;
            publish_controller_transfer_status(controller_phase, progress);
            break;
        }
        case firmware::application::FirmwareTransferEvent::completed:
            publish_controller_transfer_status(success_phase, 100U);
            notify_controller_transfer_completed(
                static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL));
            break;
        case firmware::application::FirmwareTransferEvent::error:
            notify_controller_transfer_failed();
            publish_controller_transfer_status(error_phase, 0U);
            break;
        case firmware::application::FirmwareTransferEvent::cancelled:
            notify_controller_transfer_cancelled();
            publish_controller_transfer_status(error_phase, 0U);
            break;
        case firmware::application::FirmwareTransferEvent::timed_out:
            notify_controller_transfer_timeout(true);
            publish_controller_transfer_status(error_phase, 0U);
            break;
    }
}

}  // namespace firmware::target
