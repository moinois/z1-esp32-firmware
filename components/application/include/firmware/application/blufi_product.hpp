// Declares BLUFI product-message dispatch and report payload encoding.
#pragma once

#include "firmware/application/ble_provisioning.hpp"
#include "firmware/application/blufi_wire.hpp"
#include "firmware/core/bytes.hpp"

#include <cstdint>
#include <vector>

namespace firmware::application {

// Isolates subtype selection from provisioning, security, and wire services.
class BlufiProductPort {
public:
    // Enables safe destruction through a substituted product adapter.
    virtual ~BlufiProductPort() = default;

    // Passes security negotiation bytes to the connection security context.
    virtual void receive_negotiation(core::BytesView data) = 0;

    // Requests a station connection using staged provisioning credentials.
    virtual void connect_station() = 0;

    // Requests station disconnection without clearing credentials.
    virtual void disconnect_station() = 0;

    // Requests the current Wi-Fi status product report.
    virtual void request_wifi_status() = 0;

    // Requests a scan and Wi-Fi-list product report.
    virtual void request_wifi_list() = 0;

    // Passes received station SSID bytes to provisioning policy.
    virtual void receive_ssid(core::BytesView data) = 0;

    // Passes received station password bytes to provisioning policy.
    virtual void receive_password(core::BytesView data) = 0;

    // Passes one received error value to provisioning policy.
    virtual void receive_error(std::uint8_t error) = 0;

    // Passes custom bytes to provisioning diagnostics.
    virtual void receive_custom_data(core::BytesView data) = 0;

    // Sends one outgoing logical BLUFI data message.
    virtual void send_data(std::uint8_t subtype, core::BytesView data) = 0;

    // Reports one exact BLUFI protocol error value.
    virtual void report_error(std::uint8_t error) = 0;
};

// Routes completed BLUFI control and data messages by product subtype.
class BlufiProductDispatcher {
public:
    // Binds product routing to replaceable provisioning and wire actions.
    explicit BlufiProductDispatcher(BlufiProductPort& port);

    // Applies the product effect selected by one completed incoming message.
    void dispatch(const BlufiIncomingFrame& message);

private:
    // Dispatches a recognized control subtype or ignores it.
    void dispatch_control(const BlufiIncomingFrame& message);

    // Dispatches a recognized data subtype or ignores it.
    void dispatch_data(const BlufiIncomingFrame& message);

    BlufiProductPort& port_;
};

// Encodes one exact Wi-Fi status report payload for data subtype 0x0f.
core::ByteVector encode_blufi_wifi_status(const BleWifiStatusReport& report);

// Encodes ordered Wi-Fi-list records for data subtype 0x11.
core::ByteVector encode_blufi_wifi_list(
    const std::vector<BleWifiListEntry>& entries);

}  // namespace firmware::application
