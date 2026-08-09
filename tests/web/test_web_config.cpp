// Verifies HTTP server and SPIFFS mount constants before target composition.
#include "test.hpp"

#include "application/web/web_config.hpp"

using firmware::application::main_http_server;
using firmware::application::video_http_server;
using firmware::application::web_volume;

TEST_CASE(web_001_main_and_video_servers_use_exact_independent_limits) {
    REQUIRE_EQ(main_http_server.port, 80U);
    REQUIRE_EQ(main_http_server.maximum_open_sockets, 10U);
    REQUIRE_EQ(main_http_server.send_wait_seconds, 5U);
    REQUIRE_EQ(video_http_server.port, 82U);
    REQUIRE_EQ(video_http_server.control_port, 32764U);
    REQUIRE_EQ(video_http_server.maximum_open_sockets, 7U);
    REQUIRE_EQ(video_http_server.send_wait_seconds, 10U);
}

TEST_CASE(web_001_shared_http_policy_is_exact) {
    REQUIRE_EQ(main_http_server.backlog, 5U);
    REQUIRE_EQ(main_http_server.receive_wait_seconds, 5U);
    REQUIRE(main_http_server.wildcard_uri_matching);
    REQUIRE(!main_http_server.lru_session_eviction);
    REQUIRE(!main_http_server.tcp_keepalive);
    REQUIRE(main_http_server.plaintext);
    REQUIRE(!main_http_server.authentication_required);
    REQUIRE(!main_http_server.origin_check_required);
    REQUIRE(!main_http_server.anti_forgery_token_required);
    REQUIRE(!main_http_server.prior_session_required);
    REQUIRE_EQ(firmware::application::web::request_header_capacity, 512U);
    REQUIRE_EQ(firmware::application::web::uri_capacity, 512U);
}

TEST_CASE(web_002_and_004_web_volume_mount_and_format_are_exact) {
    REQUIRE_EQ(web_volume.mount_path, std::string_view("/spiffs"));
    REQUIRE_EQ(web_volume.maximum_open_files, 5U);
    REQUIRE(web_volume.format_if_mount_fails);
    REQUIRE_EQ(web_volume.logical_block_size, 4096U);
    REQUIRE_EQ(web_volume.page_size, 256U);
    REQUIRE_EQ(web_volume.object_name_storage, 64U);
    REQUIRE_EQ(web_volume.metadata_size, 4U);
    REQUIRE(web_volume.filesystem_magic);
    REQUIRE(web_volume.validate_magic_length);
    REQUIRE(web_volume.modification_time_metadata);
}
