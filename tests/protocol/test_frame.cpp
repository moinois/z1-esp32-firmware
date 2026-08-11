// Tests the common binary envelope and transport-specific stream recovery.
#include "test.hpp"
#include "core/protocol/frame.hpp"
using firmware::core::ByteVector;
using firmware::core::Frame;
using firmware::core::StreamDecoder;
using firmware::core::StreamPolicy;
using firmware::core::UartCandidateCheckBudget;

TEST_CASE(frm_001_encoder_produces_exact_common_envelope) {
    const Frame frame{0xA1, {'?'}};
    REQUIRE_EQ(firmware::core::encode_frame(frame),
               ByteVector({0x86, 0x68, 0x00, 0x04, 0xA1, 0x3F, 0x35, 0x33, 0x55, 0xAA}));
}

TEST_CASE(frm_006_controller_update_uses_compatibility_crc_on_uart) {
    const Frame frame{0xC3U, {0xC2U}};
    const ByteVector encoded = firmware::core::encode_controller_frame(frame);
    REQUIRE_EQ(encoded,
               ByteVector({0x86U, 0x68U, 0x00U, 0x04U, 0xC3U, 0xC2U,
                           0x74U, 0xF8U, 0x55U, 0xAAU}));

    StreamDecoder controller(StreamPolicy::controller_uart());
    REQUIRE_EQ(controller.push(encoded), std::vector<Frame>({frame}));
    StreamDecoder host(StreamPolicy::tcp());
    REQUIRE(host.push(encoded).empty());
}

TEST_CASE(frm_006_controller_transport_keeps_standard_crc_outside_c_family) {
    const Frame frame{0xD3U, {0xC2U}};
    REQUIRE_EQ(firmware::core::encode_controller_frame(frame),
               firmware::core::encode_frame(frame));
}

TEST_CASE(frm_010_decoder_preserves_partial_frames_across_reads) {
    const auto encoded = firmware::core::encode_frame(Frame{0x83, {'o', 'k'}});
    StreamDecoder decoder(StreamPolicy::tcp());

    REQUIRE(decoder.push(ByteVector(encoded.begin(), encoded.begin() + 4)).empty());
    const auto frames = decoder.push(ByteVector(encoded.begin() + 4, encoded.end()));

    REQUIRE_EQ(frames.size(), 1U);
    REQUIRE_EQ(frames.front(), (Frame{0x83, {'o', 'k'}}));
}

TEST_CASE(frm_002_and_frm_003_encoder_and_decoder_enforce_length_rules) {
    const auto encoded = firmware::core::encode_frame(Frame{0x84, {}});
    REQUIRE_EQ(encoded[2], 0x00U);
    REQUIRE_EQ(encoded[3], 0x03U);

    StreamDecoder decoder(StreamPolicy::tcp());
    const auto valid = firmware::core::encode_frame(Frame{0x84, {}});
    ByteVector input{0x86, 0x68, 0x00, 0x02};
    input.insert(input.end(), valid.begin(), valid.end());
    const auto frames = decoder.push(input);
    REQUIRE_EQ(frames.size(), 1U);
    REQUIRE_EQ(frames.front().type, 0x84U);
}

TEST_CASE(frm_004_and_frm_005_reject_bad_crc_or_tail) {
    auto bad_crc = firmware::core::encode_frame(Frame{0x90, {'x'}});
    bad_crc[bad_crc.size() - 4U] ^= 1U;
    auto bad_tail = firmware::core::encode_frame(Frame{0x91, {'y'}});
    bad_tail.back() ^= 1U;

    StreamDecoder decoder(StreamPolicy::tcp());
    REQUIRE(decoder.push(bad_crc).empty());
    REQUIRE(decoder.push(bad_tail).empty());
}

TEST_CASE(frm_011_decoder_discards_noise_but_retains_unpaired_sync_byte) {
    const auto encoded = firmware::core::encode_frame(Frame{0x84, {}});
    StreamDecoder decoder(StreamPolicy::tcp());

    REQUIRE(decoder.push(ByteVector{0x00, 0x86}).empty());
    const ByteVector rest(encoded.begin() + 1, encoded.end());
    const auto frames = decoder.push(rest);

    REQUIRE_EQ(frames.size(), 1U);
    REQUIRE_EQ(frames.front().type, 0x84U);
}

TEST_CASE(frm_013_tcp_recovers_sync_inside_rejected_candidate) {
    const auto valid = firmware::core::encode_frame(Frame{0x90, {'x'}});
    ByteVector input{0x86, 0x68, 0x00, 0x02};
    input.insert(input.end(), valid.begin(), valid.end());
    StreamDecoder decoder(StreamPolicy::tcp());

    const auto frames = decoder.push(input);

    REQUIRE_EQ(frames.size(), 1U);
    REQUIRE_EQ(frames.front().payload, ByteVector({'x'}));
}

TEST_CASE(frm_012_decoder_waits_for_declared_frame) {
    const auto encoded = firmware::core::encode_frame(Frame{0x92, {'o', 'k'}});
    StreamDecoder decoder(StreamPolicy::tcp());
    REQUIRE(decoder.push(ByteVector(encoded.begin(), encoded.end() - 1)).empty());
    const auto frames = decoder.push(ByteVector{encoded.back()});
    REQUIRE_EQ(frames.size(), 1U);
    REQUIRE_EQ(frames.front().type, 0x92U);
}

TEST_CASE(frm_014_decoder_consumes_valid_frame_independent_of_destination) {
    const auto first = firmware::core::encode_frame(Frame{0x93, {'a'}});
    const auto second = firmware::core::encode_frame(Frame{0x94, {'b'}});
    ByteVector input = first;
    input.insert(input.end(), second.begin(), second.end());
    StreamDecoder decoder(StreamPolicy::tcp());
    const auto frames = decoder.push(input);
    REQUIRE_EQ(frames.size(), 2U);
    REQUIRE_EQ(frames[0].type, 0x93U);
    REQUIRE_EQ(frames[1].type, 0x94U);
}

TEST_CASE(frm_015_transport_limits_are_exact) {
    REQUIRE_EQ(StreamPolicy::controller_uart().maximum_frame_size, 528U);
    REQUIRE_EQ(StreamPolicy::tcp().maximum_frame_size, 8300U);
    REQUIRE_EQ(StreamPolicy::usb().maximum_frame_size, 8300U);
}

TEST_CASE(frm_016_usb_discards_declared_bad_candidate_in_full) {
    auto bad = firmware::core::encode_frame(Frame{0x90, {0x86, 0x68, 'x'}});
    bad[bad.size() - 4] ^= 1;
    const auto good = firmware::core::encode_frame(Frame{0x84, {}});
    bad.insert(bad.end(), good.begin(), good.end());
    StreamDecoder decoder(StreamPolicy::usb());

    const auto frames = decoder.push(bad);

    REQUIRE_EQ(frames.size(), 1U);
    REQUIRE_EQ(frames.front().type, 0x84U);
}

TEST_CASE(usb_011_single_sync_byte_has_no_timeout) {
    const auto encoded = firmware::core::encode_frame(Frame{0x84U, {}});
    StreamDecoder decoder(StreamPolicy::usb());

    REQUIRE(decoder.push(ByteVector{encoded[0]}, 0U).empty());
    const ByteVector remainder(encoded.begin() + 1, encoded.end());
    REQUIRE_EQ(decoder.push(remainder, 600000U),
               std::vector<Frame>({Frame{0x84U, {}}}));
}

TEST_CASE(usb_011_incomplete_header_expires_only_after_100_ms) {
    const auto encoded = firmware::core::encode_frame(Frame{0x84U, {}});
    StreamDecoder boundary(StreamPolicy::usb());
    REQUIRE(boundary.push(ByteVector(encoded.begin(), encoded.begin() + 3), 10U).empty());
    REQUIRE_EQ(boundary.push(ByteVector(encoded.begin() + 3, encoded.end()), 110U),
               std::vector<Frame>({Frame{0x84U, {}}}));

    StreamDecoder expired(StreamPolicy::usb());
    REQUIRE(expired.push(ByteVector(encoded.begin(), encoded.begin() + 3), 10U).empty());
    REQUIRE(expired.push(ByteVector(encoded.begin() + 3, encoded.end()), 111U).empty());
}

TEST_CASE(usb_011_body_timeout_is_extended_by_each_arriving_byte) {
    const auto encoded = firmware::core::encode_frame(Frame{0x90U, {'a', 'b'}});
    StreamDecoder decoder(StreamPolicy::usb());
    REQUIRE(decoder.push(ByteVector(encoded.begin(), encoded.begin() + 5), 0U).empty());
    REQUIRE(decoder.push(ByteVector{encoded[5]}, 10000U).empty());
    REQUIRE_EQ(decoder.push(ByteVector(encoded.begin() + 6, encoded.end()), 20000U),
               std::vector<Frame>({Frame{0x90U, {'a', 'b'}}}));
}

TEST_CASE(usb_011_body_expires_after_more_than_ten_seconds_without_a_byte) {
    const auto first = firmware::core::encode_frame(Frame{0x90U, {'a', 'b'}});
    const auto later = firmware::core::encode_frame(Frame{0x84U, {}});
    StreamDecoder decoder(StreamPolicy::usb());
    REQUIRE(decoder.push(ByteVector(first.begin(), first.begin() + 5), 0U).empty());

    REQUIRE_EQ(decoder.push(later, 10001U),
               std::vector<Frame>({Frame{0x84U, {}}}));
}

TEST_CASE(uart_009_discards_one_extra_byte_after_5000_failed_candidates) {
    UartCandidateCheckBudget budget;
    for (std::size_t check = 1U;
         check < UartCandidateCheckBudget::unsuccessful_check_limit; ++check) {
        REQUIRE(!budget.rejected_candidate());
    }
    REQUIRE(budget.rejected_candidate());
    REQUIRE(!budget.rejected_candidate());

    budget.reset();
    REQUIRE(!budget.rejected_candidate());
}

TEST_CASE(uart_009_reaching_2048_undecoded_bytes_discards_the_oldest_256) {
    const auto oldest = firmware::core::encode_controller_frame(Frame{0x84U, {}});
    ByteVector full = oldest;
    full.resize(UartCandidateCheckBudget::undecoded_capacity, 0x00U);
    StreamDecoder decoder(StreamPolicy::controller_uart());

    REQUIRE(decoder.push(full).empty());

    ByteVector with_later(UartCandidateCheckBudget::undecoded_capacity, 0x00U);
    const auto later = firmware::core::encode_controller_frame(Frame{0x85U, {}});
    std::copy(later.begin(), later.end(), with_later.end() - later.size());
    REQUIRE_EQ(decoder.push(with_later),
               std::vector<Frame>({Frame{0x85U, {}}}));
}
