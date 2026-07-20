// Verifies host file-transfer start admission and latest-value packet delivery.
#include "test.hpp"

#include "firmware/application/file_transfer_admission.hpp"

using firmware::application::FileTransferAdmission;
using firmware::application::FileTransferMailbox;
using firmware::application::HostIdentity;
using firmware::application::HostTransport;
using firmware::core::FileTransferDirection;
using firmware::core::FileTransferStart;
using firmware::core::Frame;

namespace {

HostIdentity tcp(std::uint8_t slot, std::uint32_t generation) {
    return {HostTransport::tcp, slot, generation};
}

FileTransferStart upload(const char* path) {
    return {FileTransferDirection::upload, path};
}

}  // namespace

TEST_CASE(hft_005_four_starts_wait_and_the_fifth_is_rejected) {
    FileTransferAdmission admission;

    REQUIRE(admission.enqueue(tcp(0U, 1U), upload("/one")));
    REQUIRE(admission.enqueue(tcp(1U, 1U), upload("/two")));
    REQUIRE(admission.enqueue(tcp(2U, 1U), upload("/three")));
    REQUIRE(admission.enqueue(tcp(3U, 1U), upload("/four")));
    REQUIRE(!admission.enqueue(tcp(0U, 2U), upload("/five")));
    REQUIRE_EQ(admission.pending(), 4U);
}

TEST_CASE(hft_005_pending_starts_wait_while_an_operation_is_active) {
    FileTransferAdmission admission;
    admission.enqueue(tcp(0U, 1U), upload("/one"));
    admission.enqueue(tcp(1U, 1U), upload("/two"));

    const auto first = admission.take_next();

    REQUIRE(first.has_value());
    REQUIRE_EQ(first->start.path, std::string("/one"));
    REQUIRE(!admission.take_next().has_value());
    admission.finish_active();
    REQUIRE_EQ(admission.take_next()->start.path, std::string("/two"));
}

TEST_CASE(hft_006_mailbox_replaces_an_unprocessed_owner_frame) {
    FileTransferMailbox mailbox;
    mailbox.put({0xB1U, {1U}});
    mailbox.put({0xB3U, {2U}});

    const auto selected = mailbox.take();

    REQUIRE(selected.has_value());
    REQUIRE_EQ(*selected, Frame({0xB3U, {2U}}));
    REQUIRE(!mailbox.take().has_value());
}

TEST_CASE(hft_007_queued_start_retains_full_transport_identity) {
    FileTransferAdmission admission;
    const HostIdentity original = tcp(2U, 7U);
    admission.enqueue(original, upload("/job"));

    const auto queued = admission.take_next();

    REQUIRE(queued.has_value());
    REQUIRE_EQ(queued->host, original);
}

TEST_CASE(hft_007_finishing_active_does_not_remove_an_already_queued_start) {
    FileTransferAdmission admission;
    admission.enqueue(tcp(0U, 1U), upload("/one"));
    admission.enqueue(tcp(0U, 1U), upload("/two"));
    admission.take_next();

    admission.finish_active();

    REQUIRE_EQ(admission.pending(), 1U);
    REQUIRE_EQ(admission.take_next()->start.path, std::string("/two"));
}
