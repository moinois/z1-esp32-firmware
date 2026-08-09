// Tests composition of CANopen NMT, dictionary, SDO, output, and restart effects.
#include "test.hpp"

#include "application/can/canopen_service.hpp"

#include <string>
#include <vector>

using firmware::application::CanopenService;
using firmware::application::CanopenServicePort;
using firmware::core::CanFrame;
using firmware::core::NmtState;

namespace {

class FakeCanopenServicePort final : public CanopenServicePort {
public:
    // Queues one CANopen output frame for the target bus.
    void transmit(const CanFrame& frame) override {
        transmitted.push_back(frame);
    }

    // Requests the delayed mainboard reset selected by NMT.
    void restart_mainboard() override {
        ++restart_count;
    }

    std::vector<CanFrame> transmitted;
    std::size_t restart_count = 0U;
};

// Creates one exact local expedited SDO download request.
CanFrame sdo_write(std::uint16_t index,
                   std::uint8_t subindex,
                   std::uint8_t command,
                   std::uint32_t value) {
    CanFrame frame;
    frame.identifier = firmware::core::canopen::sdo_request_identifier;
    frame.size = 8U;
    frame.data[0] = command;
    frame.data[1] = static_cast<std::uint8_t>(index);
    frame.data[2] = static_cast<std::uint8_t>(index >> 8U);
    frame.data[3] = subindex;
    frame.data[4] = static_cast<std::uint8_t>(value);
    frame.data[5] = static_cast<std::uint8_t>(value >> 8U);
    frame.data[6] = static_cast<std::uint8_t>(value >> 16U);
    frame.data[7] = static_cast<std::uint8_t>(value >> 24U);
    return frame;
}

// Creates one exact local NMT frame.
CanFrame nmt(std::uint8_t command) {
    CanFrame frame;
    frame.identifier = firmware::core::canopen::nmt_identifier;
    frame.size = 2U;
    frame.data[0] = command;
    frame.data[1] = firmware::core::canopen::node_id;
    return frame;
}

}  // namespace

TEST_CASE(can_002_service_cycle_transmits_bootup_and_sdo_heartbeat_effect) {
    FakeCanopenServicePort port;
    CanopenService service(port);

    service.process_cycle();
    REQUIRE_EQ(port.transmitted[0].data[0], 0U);

    service.receive(sdo_write(0x1017U, 0U, 0x2bU, 100U));
    REQUIRE_EQ(port.transmitted[1].data[0], 0x60U);
    service.process_cycle();
    REQUIRE_EQ(port.transmitted[2].data[0], 5U);
}

TEST_CASE(od_051_service_retains_successful_digital_output_write) {
    FakeCanopenServicePort port;
    CanopenService service(port);

    service.receive(sdo_write(0x6001U, 1U, 0x23U, 0x12345678U));

    REQUIRE_EQ(port.transmitted.back().data[0], 0x60U);
    REQUIRE_EQ(service.dictionary().read(0x6001U, 1U).data,
               firmware::core::ByteVector({0x78U, 0x56U, 0x34U, 0x12U}));
}

TEST_CASE(can_002_communication_reset_retains_dictionary_and_restarts_bootup) {
    FakeCanopenServicePort port;
    CanopenService service(port);
    service.process_cycle();
    service.receive(sdo_write(0x1017U, 0U, 0x2bU, 20U));
    service.process_cycle();

    service.receive(nmt(0x82U));
    service.process_cycle();

    REQUIRE_EQ(port.transmitted.back().data[0], 0U);
    REQUIRE_EQ(service.dictionary().read(0x1017U, 0U).data[0], 20U);
}

TEST_CASE(can_005_service_requests_mainboard_restart_after_ten_cycles) {
    FakeCanopenServicePort port;
    CanopenService service(port);
    service.process_cycle();
    service.receive(nmt(0x81U));

    for (std::size_t cycle = 0U; cycle < 9U; ++cycle) {
        service.process_cycle();
    }
    REQUIRE_EQ(port.restart_count, 0U);
    service.process_cycle();
    REQUIRE_EQ(port.restart_count, 1U);
}

TEST_CASE(can_006_service_keeps_error_register_and_nmt_state_consistent) {
    FakeCanopenServicePort port;
    CanopenService service(port);
    service.process_cycle();

    service.set_error_register(0x10U);

    REQUIRE_EQ(service.node().state(), NmtState::pre_operational);
    REQUIRE_EQ(service.dictionary().read(0x1001U, 0U).data[0], 0x10U);
}

TEST_CASE(can_002_and_006_error_transition_publishes_pre_operational_heartbeat) {
    FakeCanopenServicePort port;
    CanopenService service(port);
    service.process_cycle();
    service.receive(sdo_write(0x1017U, 0U, 0x2bU, 100U));
    service.process_cycle();
    port.transmitted.clear();

    service.set_error_register(0x10U);
    service.process_cycle();

    REQUIRE_EQ(service.node().state(), NmtState::pre_operational);
    REQUIRE_EQ(service.dictionary().read(0x1001U, 0U).data[0], 0x10U);
    REQUIRE_EQ(port.transmitted.size(), 1U);
    REQUIRE_EQ(port.transmitted[0].identifier,
               firmware::core::canopen::heartbeat_identifier);
    REQUIRE_EQ(port.transmitted[0].data[0], 127U);
}
