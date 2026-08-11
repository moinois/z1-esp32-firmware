/** @file @brief Declares exact update-phase NVS persistence diagnostics. */
#pragma once
#include <cstdint>
namespace firmware::target {
bool persist_update_phase(std::uint8_t phase);
}
