// Declares the ESP-IDF UDP discovery adapter shared by WLAN and TCP services.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace firmware::target {

// Sends a command-triggered discovery burst for the current station state.
void send_tcp_discovery_burst(std::size_t active_tcp_clients);

// Updates the station broadcast destination used by discovery policy.
void update_tcp_discovery_station(std::string_view ipv4,
                                  std::string_view netmask,
                                  std::size_t active_tcp_clients);

// Clears station-specific discovery state after a successful disconnect.
void clear_tcp_discovery_station();

// Starts the periodic discovery task after networking has been initialized.
void start_tcp_discovery_task();

}  // namespace firmware::target
