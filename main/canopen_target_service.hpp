// Declares the optional ESP-IDF task composing TWAI with portable CANopen policy.
#pragma once

#include "can_twai_adapter.hpp"

#include "firmware/application/can_output_monitor.hpp"
#include "firmware/application/canopen_service.hpp"

#include <cstdint>
#include <string_view>

namespace firmware::target {

// Owns the CAN driver, portable services, diagnostic sampler, and worker task.
class CanopenTargetService final
    : public application::CanopenServicePort,
      public application::CanOutputMonitorPort {
public:
    // Constructs all portable service state without touching target hardware.
    CanopenTargetService();

    // Initializes TWAI and starts the optional periodic worker.
    bool start();

    // Queues one CANopen output while tolerating bus-capacity failure.
    void transmit(const core::CanFrame& frame) override;

    // Restarts the mainboard after the portable NMT delay has elapsed.
    void restart_mainboard() override;

    // Writes one informational CAN diagnostic through ESP-IDF logging.
    void log_info(std::string_view tag,
                  std::string_view message) override;

private:
    // Adapts the FreeRTOS task entry point back to the owning instance.
    static void task_entry(void* context);

    // Runs immediate first-cycle processing and drift-free periodic work.
    void run();

    CanTwaiAdapter adapter_;
    application::CanopenService service_;
    application::CanOutputMonitor output_monitor_;
};

}  // namespace firmware::target
