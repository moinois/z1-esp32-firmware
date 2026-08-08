/** @file @brief Declares POSIX-backed controller transfer ports using one serialized UART. */
#pragma once

#include "firmware/application/controller_config_transfer.hpp"
#include "firmware/application/controller_factory_transfer.hpp"
#include "firmware/application/controller_firmware_transfer.hpp"

namespace firmware::target {
class ControllerChannelAdapter;

/// Provides common file reads and framed UART responses for all transfer families.
class ControllerTransferAdapter final
    : public firmware::application::ControllerFirmwarePort,
      public firmware::application::ControllerConfigPort,
      public firmware::application::ControllerFactoryPort {
public:
    /// Binds the adapter to the controller UART owned by the command task.
    explicit ControllerTransferAdapter(ControllerChannelAdapter& channel);

    bool file_exists(std::string_view path) override;
    bool configuration_available() override;
    std::optional<std::uint64_t> file_size(std::string_view path) override;
    std::optional<firmware::core::ByteVector> read_file(
        std::string_view path, std::uint64_t offset,
        std::size_t maximum_size) override;
    std::optional<std::vector<firmware::core::ByteVector>> read_chunks(
        std::string_view path, std::size_t chunk_size) override;
    std::optional<std::vector<firmware::core::ByteVector>>
    read_configuration_chunks(std::size_t chunk_size) override;
    bool remove_file(std::string_view path) override;
    bool send(firmware::core::Frame frame) override;
    void publish(firmware::application::FirmwareTransferEvent event,
                 std::uint32_t index, std::uint32_t frame_count) override;

private:
    ControllerChannelAdapter& channel_;
};

}  // namespace firmware::target
