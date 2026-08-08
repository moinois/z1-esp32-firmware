/** @file @brief Declares shared NVS-backed implementations of runtime and serial-number ports. */
#pragma once

#include "frame_sink.hpp"
#include "firmware/application/runtime_commands.hpp"
#include "firmware/application/serial_number.hpp"

namespace firmware::target {

/** Shared NVS-backed runtime-command operations independent of response transport. */
class NvsRuntimeCommandPort : public firmware::application::RuntimeCommandPort {
public:
    explicit NvsRuntimeCommandPort(FrameSink& sink);

    bool admit_operation(std::uint32_t wait_milliseconds) override;
    bool open_namespace(std::string_view name_space) override;
    firmware::application::RuntimeSignedRead read_first_boot(
        std::string_view key) override;
    std::optional<std::uint64_t> read_counter(std::string_view key) override;
    std::optional<std::string> format_utc_minute(std::int64_t seconds) override;
    firmware::application::RuntimeEraseResult erase_first_boot(
        std::string_view name_space, std::string_view key) override;
    void complete_operation() override;
    void send_response(std::uint8_t type, std::string_view payload) override;

private:
    FrameSink& sink_;
    std::string name_space_;
};

/** Shared factory serial-number persistence independent of response transport. */
class NvsSerialNumberPort : public firmware::application::SerialNumberPort {
public:
    explicit NvsSerialNumberPort(FrameSink& sink);

    bool admit_operation(std::uint32_t wait_milliseconds) override;
    firmware::application::SerialNumberRead read_serial(
        std::string_view name_space, std::string_view key) override;
    bool write_serial(std::string_view name_space, std::string_view key,
                      std::string_view value) override;
    void complete_operation() override;
    void send_response(std::uint8_t type, std::string_view payload) override;

private:
    FrameSink& sink_;
};

}  // namespace firmware::target
