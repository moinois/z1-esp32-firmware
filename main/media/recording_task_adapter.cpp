// Implements recording segment lifecycle over camera, AVI, and FAT adapters.
#include "recording_task_adapter.hpp"

#include "hardware_adapter_factory.hpp"
#include "camera_hardware_adapter.hpp"
#include "recording_file_adapter.hpp"
#include "recording_request_state.hpp"
#include "runtime_play_observer.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firmware/application/camera_settings.hpp"
#include "firmware/application/recording_policy.hpp"
#include "firmware/core/avi_writer.hpp"
#include "firmware/core/sd_user_path.hpp"

#include <ctime>
#include <cstdint>
#include <optional>
#include <string>

namespace firmware::target {
namespace {

const std::string recording_source_path =
    firmware::core::physical_sd_path("/videos/session.avi");

// Finalizes and durably writes one active in-memory AVI segment.
void close_segment(std::optional<firmware::core::AviWriter>& writer,
                   std::optional<std::string>& path) {
    if (!writer.has_value() || !path.has_value()) {
        writer.reset();
        path.reset();
        return;
    }
    const auto data = writer->finalize();
    if (data.has_value()) {
        RecordingFileAdapter{}.write_segment(*path, *data);
    }
    writer.reset();
    path.reset();
}

// Runs one recording interval and applies the shared segment policy.
void recording_task(void*) {
    RecordingRequestState request;
    std::optional<firmware::core::AviWriter> writer;
    std::optional<std::string> path;
    firmware::application::RecordingSegmentState state;
    auto& camera = HardwareAdapterFactory::camera();
    const auto& settings = camera.settings();
    const auto dimensions = firmware::application::camera_dimensions(
        settings.recording_frame_size);
    for (;;) {
        const bool active = firmware::application::recording_conditions_active(
            request.requested(), streamed_play_running(), controller_running());
        if (active && !writer.has_value()) {
            camera.set_frame_dimensions(dimensions);
            path = firmware::application::recording_segment_path(
                recording_source_path, static_cast<std::int64_t>(std::time(nullptr)));
            if (path.has_value()) {
                writer.emplace(dimensions.width, dimensions.height);
                state = {};
            }
        }
        bool capture_succeeded = false;
        bool write_succeeded = false;
        if (writer.has_value()) {
            const auto frame = camera.capture_jpeg();
            capture_succeeded = frame.has_value();
            write_succeeded = capture_succeeded && writer->append_frame(*frame);
        }
        if (writer.has_value()) {
            const bool close = firmware::application::advance_recording_segment(
                state, active, capture_succeeded, write_succeeded,
                settings.frames_per_file);
            if (close) {
                close_segment(writer, path);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(settings.frame_interval_milliseconds));
    }
}

}  // namespace

void RecordingTaskAdapter::start() {
    xTaskCreate(recording_task, "recording", 6144U, nullptr, 3U, nullptr);
}

}  // namespace firmware::target
