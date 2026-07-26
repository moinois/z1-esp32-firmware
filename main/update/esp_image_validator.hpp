// Declares structural ESP32 application-image validation for update policy.
#pragma once

#include "firmware/core/bytes.hpp"

namespace firmware::target {

// Validates an in-memory ESP image without writing it to flash.
class EspImageValidator {
public:
    // Returns true only when all segment boundaries and the checksum are valid.
    bool valid_mainboard_image(firmware::core::BytesView image) const;
};

}  // namespace firmware::target
