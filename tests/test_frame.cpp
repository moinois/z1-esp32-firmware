// Tests the common binary envelope and transport-specific stream recovery.
#include "test.hpp"
#include "firmware/core/frame.hpp"
using firmware::core::ByteVector;
using firmware::core::Frame;
using firmware::core::StreamDecoder;
using firmware::core::StreamPolicy;

TEST_CASE(frm_001_encoder_produces_exact_common_envelope) {
    const Frame frame{0xA1, {'?'}};
    REQUIRE_EQ(firmware::core::encode_frame(frame),
               ByteVector({0x86, 0x68, 0x00, 0x04, 0xA1, 0x3F, 0x35, 0x33, 0x55, 0xAA}));
}

TEST_CASE(frm_010_decoder_preserves_partial_frames_across_reads) {
    const auto encoded = firmware::core::encode_frame(Frame{0x83, {'o', 'k'}});
    StreamDecoder decoder(StreamPolicy::tcp());

    REQUIRE(decoder.push(ByteVector(encoded.begin(), encoded.begin() + 4)).empty());
    const auto frames = decoder.push(ByteVector(encoded.begin() + 4, encoded.end()));

    REQUIRE_EQ(frames.size(), 1U);
    REQUIRE_EQ(frames.front(), (Frame{0x83, {'o', 'k'}}));
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
