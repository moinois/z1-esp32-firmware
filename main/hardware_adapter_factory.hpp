// Declares the single composition point for selecting target hardware adapters.
#pragma once

namespace firmware::target {

class SdStorageAdapter;
class CameraHardwareAdapter;
class ControllerChannelAdapter;

class HardwareAdapterFactory {
public:
    // Returns the compile-time-selected SD storage adapter singleton.
    static SdStorageAdapter& sd_storage();

    // Returns the compile-time-selected camera adapter singleton.
    static CameraHardwareAdapter& camera();

    // Returns the compile-time-selected controller byte channel singleton.
    static ControllerChannelAdapter& controller_channel();

    // Returns whether test-only NVS boundary failure control is enabled.
    static bool nvs_faults_enabled();

    // Returns whether one-shot target socket failures are enabled.
    static bool network_faults_enabled();
};

}  // namespace firmware::target
