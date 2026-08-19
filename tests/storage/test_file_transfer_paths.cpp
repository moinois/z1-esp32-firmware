// Verifies host file-transfer start parsing, cache mapping, and MD5 extraction.
#include "test.hpp"

#include "core/filesystem/file_transfer_paths.hpp"

#include <string>
#include <string_view>

using firmware::core::ByteVector;
using firmware::core::FileTransferDirection;

namespace {

ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

}  // namespace

TEST_CASE(hft_001_start_parser_uses_exact_upload_and_download_offsets) {
    const auto upload = firmware::core::parse_file_transfer_start(bytes("upload /sd/u.bin"));
    const auto download = firmware::core::parse_file_transfer_start(bytes("download /sd/d.bin"));

    REQUIRE(upload.has_value());
    REQUIRE_EQ(upload->direction, FileTransferDirection::upload);
    REQUIRE_EQ(upload->path, std::string("/sd/u.bin"));
    REQUIRE(download.has_value());
    REQUIRE_EQ(download->direction, FileTransferDirection::download);
    REQUIRE_EQ(download->path, std::string("/sd/d.bin"));
}

TEST_CASE(hft_002_malformed_or_oversized_starts_are_rejected) {
    REQUIRE(!firmware::core::parse_file_transfer_start(bytes("upload ")).has_value());
    REQUIRE(!firmware::core::parse_file_transfer_start(bytes("download ")).has_value());
    REQUIRE(!firmware::core::parse_file_transfer_start(ByteVector(129U, 'x')).has_value());
}

TEST_CASE(hft_003_path_is_escaped_nul_terminated_and_trims_only_specified_edges) {
    const ByteVector payload{'u', 'p', 'l', 'o', 'a', 'd', ' ', ' ', ' ', '/', 's', 'd', '/',
                             'a', 1U, 'b', '\r', '\n', ' ', 0U, 'x'};

    const auto result = firmware::core::parse_file_transfer_start(payload);

    REQUIRE(result.has_value());
    REQUIRE_EQ(result->path, std::string("/sd/a b"));
}

TEST_CASE(hft_003_empty_upload_is_rejected_but_trimmed_empty_download_is_root) {
    REQUIRE(!firmware::core::parse_file_transfer_start(bytes("upload    ")).has_value());
    const auto download =
        firmware::core::parse_file_transfer_start(bytes("download   "));
    REQUIRE(download.has_value());
    REQUIRE_EQ(download->path, std::string("/"));
}

TEST_CASE(hft_003_tabs_are_path_bytes_and_resolved_paths_are_bounded) {
    const auto tabbed =
        firmware::core::parse_file_transfer_start(bytes("upload \tfile\t"));
    REQUIRE(tabbed.has_value());
    REQUIRE_EQ(tabbed->path, std::string("/\tfile\t"));
    const std::string payload = "upload /" + std::string(255U, 'x');
    REQUIRE(!firmware::core::parse_file_transfer_start(bytes(payload)).has_value());
}

TEST_CASE(hft_010_first_literal_gcodes_substring_selects_both_cache_paths) {
    const auto mapping =
        firmware::core::map_file_cache_paths("/sd/gcodes/jobs/a.gcode");

    REQUIRE(mapping.md5_path.has_value());
    REQUIRE(mapping.compressed_path.has_value());
    REQUIRE_EQ(*mapping.md5_path, std::string("/sd/gcodes/.md5/jobs/a.gcode"));
    REQUIRE_EQ(*mapping.compressed_path, std::string("/sd/gcodes/.lz/jobs/a.gcode"));

    const auto embedded =
        firmware::core::map_file_cache_paths("/sd/prefixgcodes/jobs/a.gcode");
    REQUIRE(embedded.md5_path.has_value());
    REQUIRE_EQ(*embedded.md5_path,
               std::string("/sd/gcodes/.md5/jobs/a.gcode"));
    REQUIRE_EQ(*embedded.compressed_path,
               std::string("/sd/gcodes/.lz/jobs/a.gcode"));

    const auto outside_sd =
        firmware::core::map_file_cache_paths("/elsewhere/gcodes/job.nc");
    REQUIRE_EQ(*outside_sd.md5_path,
               std::string("/sd/gcodes/.md5/job.nc"));
}

TEST_CASE(hft_011_sd_paths_without_gcodes_have_only_a_root_md5_mapping) {
    const auto mapping = firmware::core::map_file_cache_paths("/sd/config.txt");

    REQUIRE_EQ(*mapping.md5_path, std::string("/sd/.md5/config.txt"));
    REQUIRE(!mapping.compressed_path.has_value());

    const auto other = firmware::core::map_file_cache_paths("/flash/file.bin");
    REQUIRE(!other.md5_path.has_value());
    REQUIRE(!other.compressed_path.has_value());
}

TEST_CASE(hft_013_md5_extraction_collects_first_32_hex_characters_from_63_bytes) {
    const std::string content = "xx 01-23-45-67-89-AB-CD-EF-01-23-45-67-89-AB-CD-EF trailing";

    const auto md5 = firmware::core::extract_cached_md5(bytes(content));

    REQUIRE(md5.has_value());
    REQUIRE_EQ(*md5, std::string("0123456789abcdef0123456789abcdef"));
}

TEST_CASE(hft_013_md5_extraction_ignores_bytes_after_the_first_63) {
    ByteVector content(63U, 'z');
    const ByteVector suffix = bytes("0123456789abcdef0123456789abcdef");
    content.insert(content.end(), suffix.begin(), suffix.end());

    REQUIRE(!firmware::core::extract_cached_md5(content).has_value());
}
