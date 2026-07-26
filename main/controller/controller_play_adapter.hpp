// Declares the POSIX file and UART port used by streamed-play controller logic.
#pragma once

#include "firmware/application/play_controller.hpp"
#include "posix_file.hpp"
#include <string>

namespace firmware::target {
class ControllerUartAdapter;

// Bridges PlayControllerPort and PlayLineSource to one controller UART task.
class ControllerPlayAdapter final
    : public firmware::application::PlayControllerPort {
public:
    // Binds play responses and file reads to the controller UART.
    explicit ControllerPlayAdapter(ControllerUartAdapter& uart);
    ~ControllerPlayAdapter() override;

    void close_file() override;
    std::optional<std::uint64_t> open_file(std::string_view path) override;
    std::optional<std::string> cached_md5(std::string_view path) override;
    void broadcast(firmware::core::Frame frame) override;
    bool send(firmware::core::Frame frame) override;
    void play_state_changed(bool running) override;
    void release_play_ownership() override;
    bool rewind_file() override;
    std::uint64_t now_milliseconds() const override;
    std::optional<firmware::application::PlayLineChunk> read_chunk(
        std::size_t maximum_size) override;

private:
    ControllerUartAdapter& uart_;
    PosixFile file_;
};

}  // namespace firmware::target
