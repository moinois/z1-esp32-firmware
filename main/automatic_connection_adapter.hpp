// Declares the ESP-IDF adapter for saved-credential station startup.
#pragma once

#include "firmware/application/automatic_connection.hpp"

namespace firmware::target {

// Owns the platform task used by the portable automatic reconnect policy.
class AutomaticConnectionAdapter final
    : public firmware::application::AutomaticConnectionPort {
public:
    // Loads saved credentials and schedules the initial delayed association.
    void start(firmware::application::StationRuntime& runtime);

    // Applies one station-disconnect event to the retry policy.
    void on_station_disconnected();

    firmware::application::StoredString read_string(
        std::string_view name_space, std::string_view key) override;
    firmware::application::StationApiResult apply_station_config(
        const firmware::application::StationConfiguration& configuration) override;
    bool schedule_automatic_connection(std::uint32_t delay_milliseconds) override;
    firmware::application::StationApiResult request_connect() override;
    void delay_milliseconds(std::uint32_t duration) override;

private:
    static void delayed_connect_task(void* context);
    firmware::application::StationRuntime* runtime_ = nullptr;
    std::uint32_t scheduled_delay_milliseconds_ = 0U;
};

}  // namespace firmware::target
