// Verifies static-file lookup and 256-byte chunked response behavior.
#include "test.hpp"

#include "firmware/application/static_file_server.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::StaticFilePort;
using firmware::application::StaticFileResponsePort;
using firmware::application::StaticFileServer;
using firmware::core::ByteVector;

namespace {

class FakeStaticFilePort final : public StaticFilePort {
public:
    std::optional<std::uint64_t> open(std::string_view path) override {
        opened_path = std::string(path);
        if (!file_exists) {
            return std::nullopt;
        }
        offset = 0U;
        return content.size();
    }

    std::optional<ByteVector> read(std::size_t maximum_bytes) override {
        if (offset >= content.size()) {
            return ByteVector{};
        }
        const std::size_t count =
            std::min(maximum_bytes, content.size() - offset);
        ByteVector result(content.begin() + static_cast<std::ptrdiff_t>(offset),
                          content.begin() + static_cast<std::ptrdiff_t>(offset + count));
        offset += count;
        return result;
    }

    void close() override {
        ++close_calls;
    }

    bool file_exists = true;
    std::string content;
    std::string opened_path;
    std::size_t offset = 0U;
    std::size_t close_calls = 0U;
};

class FakeStaticFileResponse final : public StaticFileResponsePort {
public:
    void send_error(std::uint16_t status, std::string_view content_type,
                    std::string_view body) override {
        error_status = status;
        error_type = content_type;
        error_body = body;
    }

    void begin_chunked(std::string_view content_type) override {
        chunked_type = content_type;
        began_chunks = true;
    }

    void send_chunk(firmware::core::BytesView chunk) override {
        chunks.emplace_back(chunk.begin(), chunk.end());
    }

    void finish_chunks() override {
        finished_chunks = true;
    }

    std::uint16_t error_status = 0U;
    std::string_view error_type{};
    std::string_view error_body{};
    std::string_view chunked_type{};
    std::vector<ByteVector> chunks;
    bool began_chunks = false;
    bool finished_chunks = false;
};

}  // namespace

TEST_CASE(web_011_missing_or_overlong_static_files_return_exact_404) {
    FakeStaticFilePort file;
    FakeStaticFileResponse response;
    StaticFileServer server;
    file.file_exists = false;
    server.serve("/missing.html", file, response);
    REQUIRE_EQ(response.error_status, 404U);
    REQUIRE_EQ(response.error_type, std::string_view("text/html"));
    REQUIRE_EQ(response.error_body,
               std::string_view("Nothing matches the given URI"));

    response = {};
    const std::string overlong(300U, 'x');
    server.serve(overlong, file, response);
    REQUIRE_EQ(response.error_status, 404U);
}

TEST_CASE(web_011_existing_files_use_mime_type_and_256_byte_chunks) {
    FakeStaticFilePort file;
    FakeStaticFileResponse response;
    StaticFileServer server;
    file.content.assign(600U, 'x');
    server.serve("/assets/app.js", file, response);
    REQUIRE_EQ(file.opened_path, std::string("/spiffs/assets/app.js"));
    REQUIRE(response.began_chunks);
    REQUIRE_EQ(response.chunked_type, std::string_view("application/javascript"));
    REQUIRE_EQ(response.chunks.size(), 3U);
    REQUIRE_EQ(response.chunks[0].size(), 256U);
    REQUIRE_EQ(response.chunks[1].size(), 256U);
    REQUIRE_EQ(response.chunks[2].size(), 88U);
    REQUIRE(response.finished_chunks);
    REQUIRE_EQ(file.close_calls, 1U);
}

TEST_CASE(web_011_directory_uri_serves_index_file) {
    FakeStaticFilePort file;
    FakeStaticFileResponse response;
    StaticFileServer server;
    file.content = "index";
    server.serve("/", file, response);
    REQUIRE_EQ(file.opened_path, std::string("/spiffs/index.html"));
    REQUIRE_EQ(response.chunks.size(), 1U);
}
