// Declares the single composition point for selecting target hardware adapters.
#pragma once

namespace firmware::target {

class SdStorageAdapter;
class CameraHardwareAdapter;

class HardwareAdapterFactory {
public:
    // Returns the compile-time-selected SD storage adapter singleton.
    static SdStorageAdapter& sd_storage();

    // Returns the compile-time-selected camera adapter singleton.
    static CameraHardwareAdapter& camera();
};

}  // namespace firmware::target
