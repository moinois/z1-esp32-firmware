// Declares ESP-IDF station operations for a TCP-origin WLAN command.
#pragma once

#include "firmware/application/station_connection.hpp"

namespace firmware::target {

class TcpWlanStationAdapter final
    : public firmware::application::StationConnectionPort {
public:
    // Provides replaceable station operations over ESP-IDF Wi-Fi APIs.
    TcpWlanStationAdapter() = default;

    firmware::application::StationApiResult request_disconnect() override;
    firmware::application::StationApiResult apply_station_config(
        const firmware::application::StationConfiguration& configuration) override;
    firmware::application::StationApiResult request_connect() override;
    void delay_milliseconds(std::uint32_t duration) override;
    firmware::application::StationSnapshot station_snapshot() const override;
    firmware::application::StationApiResult save_credentials(
        std::string_view ssid, std::string_view password) override;

    // Returns the current STA netmask in dotted-decimal form.
    std::string current_netmask() const;
};

}  // namespace firmware::target
