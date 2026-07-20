// Verifies BLUFI fragment sizing, prefixes, reassembly, and retained failures.
#include "test.hpp"

#include "firmware/application/blufi_fragment.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

using firmware::application::BlufiFragmentPort;
using firmware::application::BlufiFragmentSession;
using firmware::application::BlufiFrameType;
using firmware::application::BlufiIncomingFrame;
using firmware::core::ByteVector;

namespace {

// Records one logical outgoing fragment passed toward the BLUFI wire layer.
struct SentFragment {
    std::uint8_t subtype;
    ByteVector data;
    bool non_final;
};

// Substitutes notification, allocation, and error behavior for fragment tests.
class FakeBlufiFragmentPort final : public BlufiFragmentPort {
public:
    // Records one outgoing data frame and reports the configured outcome.
    bool send_data(std::uint8_t subtype, firmware::core::BytesView data,
                   bool non_final) override {
        sent.push_back({subtype, ByteVector(data.begin(), data.end()),
                        non_final});
        return send_succeeds;
    }

    // Returns an exact zeroed message allocation unless failure is requested.
    std::optional<ByteVector> allocate_message(std::size_t size) override {
        allocation_sizes.push_back(size);
        if (allocation_fails) {
            return std::nullopt;
        }
        return ByteVector(size, 0U);
    }

    // Records one exact BLUFI protocol error.
    void report_error(std::uint8_t error) override {
        errors.push_back(error);
    }

    bool send_succeeds = true;
    bool allocation_fails = false;
    std::vector<SentFragment> sent;
    std::vector<std::size_t> allocation_sizes;
    std::vector<std::uint8_t> errors;
};

// Creates a wire-validated frame for fragment reassembly tests.
BlufiIncomingFrame incoming(std::uint8_t subtype, ByteVector data,
                            bool non_final,
                            BlufiFrameType type = BlufiFrameType::data) {
    return {type, subtype, std::move(data), non_final};
}

// Creates deterministic ascending bytes of the requested length.
ByteVector ascending_bytes(std::size_t size) {
    ByteVector bytes;
    bytes.reserve(size);
    for (std::size_t index = 0U; index < size; ++index) {
        bytes.push_back(static_cast<std::uint8_t>(index));
    }
    return bytes;
}

}  // namespace

TEST_CASE(bwf_030_to_031_default_mtu_sends_exact_remaining_prefixes) {
    FakeBlufiFragmentPort port;
    BlufiFragmentSession session(port);
    const ByteVector message = ascending_bytes(30U);

    REQUIRE(session.send_data(0x13U, message));

    REQUIRE_EQ(port.sent.size(), 3U);
    REQUIRE_EQ(port.sent[0].subtype, 0x13U);
    REQUIRE(port.sent[0].non_final);
    REQUIRE_EQ(port.sent[0].data,
               ByteVector({30U, 0U, 0U, 1U, 2U, 3U, 4U, 5U, 6U,
                           7U, 8U, 9U, 10U, 11U}));
    REQUIRE(port.sent[1].non_final);
    REQUIRE_EQ(port.sent[1].data,
               ByteVector({18U, 0U, 12U, 13U, 14U, 15U, 16U, 17U,
                           18U, 19U, 20U, 21U, 22U, 23U}));
    REQUIRE(!port.sent[2].non_final);
    REQUIRE_EQ(port.sent[2].data,
               ByteVector({24U, 25U, 26U, 27U, 28U, 29U}));
}

TEST_CASE(bwf_031_mtu_is_capped_at_255_and_exact_capacity_is_unfragmented) {
    FakeBlufiFragmentPort port;
    BlufiFragmentSession session(port);
    session.set_att_mtu(300U);

    REQUIRE(session.send_data(2U, ascending_bytes(244U)));
    REQUIRE_EQ(port.sent.size(), 1U);
    REQUIRE(!port.sent[0].non_final);

    port.sent.clear();
    REQUIRE(session.send_data(2U, ascending_bytes(245U)));
    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE(port.sent[0].non_final);
    REQUIRE_EQ(port.sent[0].data.size(), 246U);
    REQUIRE_EQ(port.sent[0].data[0], 245U);
    REQUIRE_EQ(port.sent[0].data[1], 0U);
    REQUIRE_EQ(port.sent[1].data, ByteVector({244U}));
}

TEST_CASE(bwf_030_rejects_logical_lengths_that_do_not_fit_prefix) {
    FakeBlufiFragmentPort port;
    BlufiFragmentSession session(port);

    REQUIRE(!session.send_data(1U, ByteVector(65536U, 0U)));

    REQUIRE(port.sent.empty());
    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({9U}));
}

TEST_CASE(bwf_032_reassembles_non_final_fragments_and_exact_final_data) {
    FakeBlufiFragmentPort port;
    BlufiFragmentSession session(port);

    REQUIRE(!session.receive(
                 incoming(2U, {5U, 0U, 'a', 'b'}, true))
                 .has_value());
    REQUIRE(!session.receive(
                 incoming(2U, {3U, 0U, 'c', 'd'}, true))
                 .has_value());
    const auto complete = session.receive(incoming(2U, {'e'}, false));

    REQUIRE(complete.has_value());
    REQUIRE_EQ(complete->type, BlufiFrameType::data);
    REQUIRE_EQ(complete->subtype, 2U);
    REQUIRE_EQ(complete->data, ByteVector({'a', 'b', 'c', 'd', 'e'}));
    REQUIRE(!complete->non_final_fragment);
    REQUIRE_EQ(port.allocation_sizes, std::vector<std::size_t>({5U}));
    REQUIRE(port.errors.empty());
}

TEST_CASE(bwf_032_allocation_failure_and_missing_final_buffer_report_error_five) {
    FakeBlufiFragmentPort port;
    port.allocation_fails = true;
    BlufiFragmentSession session(port);

    REQUIRE(!session.receive(incoming(2U, {2U, 0U, 'a'}, true))
                 .has_value());
    REQUIRE(!session.receive(incoming(2U, {'b'}, false)).has_value());

    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({5U, 5U}));
}

TEST_CASE(bwf_032_zero_offset_errors_retain_allocation_for_later_final_data) {
    FakeBlufiFragmentPort port;
    BlufiFragmentSession session(port);

    REQUIRE(!session.receive(incoming(2U, {1U, 0U, 'a', 'b'}, true))
                 .has_value());
    REQUIRE(!session.receive(incoming(2U, {1U, 0U, 'c'}, true))
                 .has_value());
    const auto complete = session.receive(incoming(2U, {'d'}, false));

    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({9U, 12U}));
    REQUIRE(complete.has_value());
    REQUIRE_EQ(complete->data, ByteVector({'d'}));
}

TEST_CASE(bwf_032_overflow_and_final_mismatch_retain_accumulated_content) {
    FakeBlufiFragmentPort port;
    BlufiFragmentSession session(port);

    REQUIRE(!session.receive(incoming(2U, {3U, 0U, 'a', 'b'}, true))
                 .has_value());
    REQUIRE(!session.receive(incoming(2U, {1U, 0U, 'x', 'y'}, true))
                 .has_value());
    REQUIRE(!session.receive(incoming(2U, {}, false)).has_value());
    const auto complete = session.receive(incoming(2U, {'c'}, false));

    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({9U, 9U}));
    REQUIRE(complete.has_value());
    REQUIRE_EQ(complete->data, ByteVector({'a', 'b', 'c'}));
}

TEST_CASE(bwf_032_fragment_type_or_subtype_mismatch_retains_partial_message) {
    FakeBlufiFragmentPort port;
    BlufiFragmentSession session(port);

    REQUIRE(!session.receive(incoming(2U, {2U, 0U, 'a'}, true))
                 .has_value());
    REQUIRE(!session.receive(incoming(3U, {'x'}, false)).has_value());
    const auto complete = session.receive(incoming(2U, {'b'}, false));

    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({9U}));
    REQUIRE(complete.has_value());
    REQUIRE_EQ(complete->data, ByteVector({'a', 'b'}));
}

TEST_CASE(bwf_004_fragment_reset_discards_partial_connection_state) {
    FakeBlufiFragmentPort port;
    BlufiFragmentSession session(port);

    REQUIRE(!session.receive(incoming(2U, {2U, 0U, 'a'}, true))
                 .has_value());
    session.reset();
    const auto ordinary = session.receive(incoming(3U, {'b'}, false));

    REQUIRE(ordinary.has_value());
    REQUIRE_EQ(ordinary->subtype, 3U);
    REQUIRE_EQ(ordinary->data, ByteVector({'b'}));
}
