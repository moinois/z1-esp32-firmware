// Implements bytewise configuration copies using POSIX VFS files.
#include "tcp_configuration_file_adapter.hpp"
#include "configuration_file_store.hpp"

#include "firmware/application/tcp_client_session.hpp"

#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace firmware::target {

std::string_view TcpConfigurationFileAdapter::active_configuration_path() const {
    return "/sd/config.txt";
}

std::string_view TcpConfigurationFileAdapter::default_configuration_path() const {
    return "/sd/config.default";
}

TcpConfigurationFileAdapter::TcpConfigurationFileAdapter(
    firmware::application::TcpClientSession& session) : session_(session) {}

TcpConfigurationFileAdapter::~TcpConfigurationFileAdapter() {
    close_source();
    close_destination();
}

bool TcpConfigurationFileAdapter::file_exists(std::string_view path) {
    struct stat information{};
    return stat(std::string(path).c_str(), &information) == 0 &&
           S_ISREG(information.st_mode);
}

bool TcpConfigurationFileAdapter::open_source(std::string_view path) {
    close_source();
    source_ = std::fopen(std::string(path).c_str(), "rb");
    return source_ != nullptr;
}

bool TcpConfigurationFileAdapter::open_truncated_destination(
    std::string_view path) {
    close_destination();
    destination_ = std::fopen(std::string(path).c_str(), "wb");
    return destination_ != nullptr;
}

firmware::application::ByteRead TcpConfigurationFileAdapter::read_byte() {
    if (source_ == nullptr) {
        return {firmware::application::ByteReadStatus::failure, 0U};
    }
    std::uint8_t value = 0U;
    const std::size_t count = std::fread(&value, 1U, 1U, source_);
    if (count == 1U) {
        return {firmware::application::ByteReadStatus::byte, value};
    }
    return {std::ferror(source_) != 0
                ? firmware::application::ByteReadStatus::failure
                : firmware::application::ByteReadStatus::end_of_file,
            0U};
}

bool TcpConfigurationFileAdapter::write_byte(std::uint8_t value) {
    return destination_ != nullptr &&
           std::fwrite(&value, 1U, 1U, destination_) == 1U;
}

void TcpConfigurationFileAdapter::close_source() {
    if (source_ != nullptr) std::fclose(source_);
    source_ = nullptr;
}

bool TcpConfigurationFileAdapter::close_destination() {
    if (destination_ == nullptr) return true;
    const bool flushed = std::fflush(destination_) == 0;
    const bool closed = std::fclose(destination_) == 0;
    destination_ = nullptr;
    return flushed && closed;
}

void TcpConfigurationFileAdapter::send(firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

}  // namespace firmware::target
