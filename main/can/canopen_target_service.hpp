/** @file @brief Declares the optional ESP-IDF task composing TWAI with portable CANopen policy. */
#pragma once

#include "can_twai_adapter.hpp"

#include "firmware/application/can_output_monitor.hpp"
#include "firmware/application/canopen_service.hpp"
#include "firmware/core/canopen_sdo_mailbox.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <cstdint>
#include <string_view>

namespace firmware::target {

/// Owns the CAN driver, portable services, diagnostic sampler, and worker task.
class CanopenTargetService final
    : public application::CanopenServicePort,
      public application::CanOutputMonitorPort {
public:
    /// Constructs all portable service state without touching target hardware.
    CanopenTargetService();

    /// Initializes TWAI and starts the optional periodic worker.
    bool start();

    /// Queues one CANopen output while tolerating bus-capacity failure.
    void transmit(const core::CanFrame& frame) override;

    /// Restarts the mainboard after the portable NMT delay has elapsed.
    void restart_mainboard() override;

    /// Writes one informational CAN diagnostic through ESP-IDF logging.
    void log_info(std::string_view tag,
                  std::string_view message) override;

    /// Performs one serialized expedited SDO upload against a remote node.
    std::optional<std::uint32_t> read_remote_u32(
        std::uint8_t node, std::uint16_t index, std::uint8_t subindex,
        std::uint32_t timeout_milliseconds);

    /// Performs one serialized expedited SDO download against a remote node.
    bool write_remote_u32(std::uint8_t node, std::uint16_t index,
                          std::uint8_t subindex, std::uint32_t value,
                          std::uint32_t timeout_milliseconds);

private:
    /// Adapts the FreeRTOS task entry point back to the owning instance.
    static void task_entry(void* context);

    /// Runs immediate first-cycle processing and drift-free periodic work.
    void run();

    CanTwaiAdapter adapter_;
    application::CanopenService service_;
    application::CanOutputMonitor output_monitor_;
    core::CanopenSdoMailbox sdo_mailbox_;
    SemaphoreHandle_t sdo_mutex_ = nullptr;
    SemaphoreHandle_t sdo_response_ = nullptr;
    std::optional<core::SdoClientResponse> sdo_result_;
};

/// Returns the service instance started by the composition root, when available.
CanopenTargetService* active_canopen_target_service();

}  // namespace firmware::target
