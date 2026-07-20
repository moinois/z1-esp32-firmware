// Defines target-neutral HTTP server and SPIFFS volume configuration.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace firmware::application {

namespace web {

inline constexpr std::size_t request_header_capacity = 512U;
inline constexpr std::size_t uri_capacity = 512U;

}  // namespace web

// Groups listener and session limits for one plaintext HTTP server.
struct HttpServerConfig {
    std::uint16_t port;
    std::uint16_t control_port;
    std::size_t maximum_open_sockets;
    std::size_t backlog;
    std::uint32_t receive_wait_seconds;
    std::uint32_t send_wait_seconds;
    bool wildcard_uri_matching;
    bool lru_session_eviction;
    bool tcp_keepalive;
    bool plaintext;
    bool authentication_required;
    bool origin_check_required;
    bool anti_forgery_token_required;
    bool prior_session_required;
};

inline constexpr HttpServerConfig main_http_server{
    80U, 32768U, 10U, 5U, 5U, 5U, true, false, false, true,
    false, false, false, false};
inline constexpr HttpServerConfig video_http_server{
    82U, 32764U, 7U, 5U, 5U, 10U, false, false, false, true,
    false, false, false, false};

// Groups mount and on-flash format settings for the independent web volume.
struct WebVolumeConfig {
    std::string_view mount_path;
    std::size_t maximum_open_files;
    bool format_if_mount_fails;
    std::size_t logical_block_size;
    std::size_t page_size;
    std::size_t object_name_storage;
    std::size_t metadata_size;
    bool filesystem_magic;
    bool validate_magic_length;
    bool modification_time_metadata;
};

inline constexpr WebVolumeConfig web_volume{
    "/spiffs", 5U, true, 4096U, 256U, 64U, 4U, true, true, true};

}  // namespace firmware::application
