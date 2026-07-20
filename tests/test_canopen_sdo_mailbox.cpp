// Verifies one-operation SDO mailbox admission, matching, and deadlines.
#include "test.hpp"

#include "firmware/core/canopen_sdo_mailbox.hpp"

TEST_CASE(sdo_mailbox_rejects_second_operation_until_response) {
    firmware::core::CanopenSdoMailbox mailbox;
    REQUIRE(mailbox.begin_upload(1U, 0x6000U, 1U, 10U, 100U).has_value());
    REQUIRE(!mailbox.begin_download(1U, 0x6001U, 1U, 1U, 11U, 100U).has_value());
    REQUIRE(mailbox.pending());
}

TEST_CASE(sdo_mailbox_clears_on_matching_value_or_abort) {
    firmware::core::CanopenSdoMailbox mailbox;
    REQUIRE(mailbox.begin_upload(1U, 0x6000U, 1U, 0U, 50U).has_value());
    firmware::core::CanFrame response{
        0x581U, {0x43U, 0x00U, 0x60U, 1U, 7U, 0U, 0U, 0U}, 8U};
    const auto value = mailbox.accept(response);
    REQUIRE(value.has_value());
    REQUIRE_EQ(value->value, 7U);
    REQUIRE(!mailbox.pending());

    REQUIRE(mailbox.begin_upload(1U, 0x6000U, 1U, 0U, 50U).has_value());
    response.data[0] = 0x80U;
    const auto abort = mailbox.accept(response);
    REQUIRE(abort.has_value());
    REQUIRE(abort->aborted);
    REQUIRE(!mailbox.pending());
}

TEST_CASE(sdo_mailbox_ignores_unrelated_response_and_times_out) {
    firmware::core::CanopenSdoMailbox mailbox;
    REQUIRE(mailbox.begin_upload(1U, 0x6000U, 1U, 100U, 20U).has_value());
    const firmware::core::CanFrame unrelated{
        0x581U, {0x43U, 1U, 0x60U, 1U, 0U, 0U, 0U, 0U}, 8U};
    REQUIRE(!mailbox.accept(unrelated).has_value());
    REQUIRE(!mailbox.timed_out(119U));
    REQUIRE(mailbox.timed_out(120U));
    REQUIRE(!mailbox.pending());
}
