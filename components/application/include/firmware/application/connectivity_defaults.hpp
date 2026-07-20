// Defines connectivity defaults shared only within the application partition.
#pragma once

#include <cstdint>
#include <string_view>

namespace firmware::application::connectivity_defaults {

inline constexpr std::string_view access_point_ipv4 = "192.168.4.1";
inline constexpr std::string_view access_point_broadcast = "192.168.4.255";
inline constexpr std::uint8_t access_point_prefix_length = 24U;
inline constexpr std::uint8_t maximum_access_point_clients = 4U;
inline constexpr std::uint8_t global_socket_capacity = 16U;
inline constexpr std::string_view network_hostname = "espressif";

}  // namespace firmware::application::connectivity_defaults
