// Declares one composed BLUFI connection across wire, fragments, and products.
#pragma once

#include "firmware/application/blufi_fragment.hpp"
#include "firmware/application/blufi_product.hpp"
#include "firmware/application/blufi_security.hpp"
#include "firmware/application/blufi_wire.hpp"
#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace firmware::application {

// Receives only the product actions selected by a composed BLUFI connection.
class BlufiProductActions {
public:
    // Enables safe destruction through a substituted product action adapter.
    virtual ~BlufiProductActions() = default;

    // Marks provisioning commands usable after successful negotiation.
    virtual void security_negotiated() = 0;

    // Requests a station connection with staged credentials.
    virtual void connect_station() = 0;

    // Requests station disconnection without clearing credentials.
    virtual void disconnect_station() = 0;

    // Requests a current Wi-Fi status report.
    virtual void request_wifi_status() = 0;

    // Requests a Wi-Fi scan and list report.
    virtual void request_wifi_list() = 0;

    // Passes station SSID bytes to provisioning policy.
    virtual void receive_ssid(core::BytesView data) = 0;

    // Passes station password bytes to provisioning policy.
    virtual void receive_password(core::BytesView data) = 0;

    // Passes one received protocol error to provisioning policy.
    virtual void receive_error(std::uint8_t error) = 0;

    // Passes custom product bytes to diagnostic policy.
    virtual void receive_custom_data(core::BytesView data) = 0;
};

// Isolates the composed protocol from characteristic delivery and allocation.
class BlufiSessionTransport {
public:
    // Enables safe destruction through a substituted session transport.
    virtual ~BlufiSessionTransport() = default;

    // Delivers one completely encoded outgoing characteristic value.
    virtual void send_characteristic(core::BytesView frame) = 0;

    // Allocates one exact zeroed fragmented-message buffer.
    virtual std::optional<core::ByteVector> allocate_message(
        std::size_t size) = 0;
};

// Composes all portable BLUFI stages for one BLE connection.
class BlufiConnectionSession final : private BlufiWirePort,
                                     private BlufiFragmentPort,
                                     private BlufiProductPort {
public:
    // Binds crypto, negotiation, product actions, and transport adapters.
    BlufiConnectionSession(BlufiCipher& cipher,
                           BlufiNegotiationHandler& negotiation,
                           BlufiProductActions& actions,
                           BlufiSessionTransport& transport);

    // Resets sequences, security mode, fragments, and readiness notification.
    void reset();

    // Applies the negotiated ATT MTU to outgoing fragmentation.
    void set_att_mtu(std::uint16_t mtu);

    // Validates and processes one complete incoming characteristic value.
    void receive_characteristic(core::BytesView frame);

    // Sends one outgoing logical product data message.
    bool send_product_data(std::uint8_t subtype, core::BytesView data);

    // Sends one outgoing BLUFI error data message.
    void report_protocol_error(std::uint8_t error);

private:
    // Forwards an encoded wire frame to the characteristic transport.
    void send_characteristic(core::BytesView frame) override;

    // Sends one data frame from the fragment layer through the wire layer.
    bool send_data(std::uint8_t subtype, core::BytesView data,
                   bool non_final) override;

    // Delegates partial-message allocation to the outer transport.
    std::optional<core::ByteVector> allocate_message(
        std::size_t size) override;

    // Routes every protocol-layer error through BLUFI error data.
    void report_error(std::uint8_t error) override;

    // Processes security negotiation and publishes its first ready transition.
    void receive_negotiation(core::BytesView data) override;

    // Delegates a station-connect product action.
    void connect_station() override;

    // Delegates a station-disconnect product action.
    void disconnect_station() override;

    // Delegates a Wi-Fi status product action.
    void request_wifi_status() override;

    // Delegates a Wi-Fi-list product action.
    void request_wifi_list() override;

    // Delegates received station SSID bytes.
    void receive_ssid(core::BytesView data) override;

    // Delegates received station password bytes.
    void receive_password(core::BytesView data) override;

    // Delegates one received error value.
    void receive_error(std::uint8_t error) override;

    // Delegates custom diagnostic bytes.
    void receive_custom_data(core::BytesView data) override;

    // Sends product-dispatch output through fragmentation.
    void send_data(std::uint8_t subtype, core::BytesView data) override;

    BlufiCipher& cipher_;
    BlufiNegotiationHandler& negotiation_;
    BlufiProductActions& actions_;
    BlufiSessionTransport& transport_;
    BlufiWireSession wire_;
    BlufiFragmentSession fragments_;
    BlufiProductDispatcher products_;
    bool security_ready_notified_ = false;
};

}  // namespace firmware::application
