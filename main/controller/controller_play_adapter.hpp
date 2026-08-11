/** @file @brief Declares the POSIX file and UART port used by streamed-play controller logic. */
#pragma once

#include "application/playback/play_controller.hpp"
#include "posix_file.hpp"
#include <string>

namespace firmware::target {
class ControllerChannelAdapter;

/// Bridges PlayControllerPort and PlayLineSource to one controller UART task.
class ControllerPlayAdapter final
    : public firmware::application::PlayControllerPort {
public:
    /// Binds play responses and file reads to the controller UART.
    explicit ControllerPlayAdapter(ControllerChannelAdapter& channel);
    ~ControllerPlayAdapter() override;

    void close_file() override;
    std::optional<std::uint64_t> open_file(std::string_view path) override;
    std::optional<std::string> cached_md5(std::string_view path) override;
    void broadcast(firmware::core::Frame frame) override;
    void diagnose(const firmware::application::PlaybackDiagnostic& diagnostic) override;
    bool send(firmware::core::Frame frame) override;
    void play_state_changed(bool running) override;
    void release_play_ownership() override;
    bool rewind_file() override;
    std::uint64_t now_milliseconds() const override;
    std::optional<firmware::application::PlayLineChunk> read_chunk(
        std::size_t maximum_size) override;

private:
    ControllerChannelAdapter& channel_;
    PosixFile file_;
};

}  // namespace firmware::target
