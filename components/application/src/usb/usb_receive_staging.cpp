/** @file @brief Implements USB receive staging with whole-block capacity admission. */
#include "firmware/application/usb_receive_staging.hpp"

namespace firmware::application {

bool UsbReceiveStaging::stage(core::BytesView block) {
    if (block.size() > capacity || bytes_.size() > capacity - block.size()) {
        clear();
        return false;
    }
    bytes_.insert(bytes_.end(), block.begin(), block.end());
    return true;
}

core::ByteVector UsbReceiveStaging::take() {
    core::ByteVector result = std::move(bytes_);
    bytes_.clear();
    return result;
}

void UsbReceiveStaging::clear() {
    bytes_.clear();
}

std::size_t UsbReceiveStaging::size() const {
    return bytes_.size();
}

}  // namespace firmware::application
