// Verifies bounded lazy loading, lookup, duplicate, and update semantics.
#include "test.hpp"

#include "firmware/application/live_configuration.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::LiveConfiguration;
using firmware::application::LiveConfigurationPort;
using firmware::core::ByteVector;

namespace {

// Converts text chunks to byte vectors for configured source results.
ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

// Supplies deterministic configuration chunks without a filesystem.
class FakeLiveConfigurationPort final : public LiveConfigurationPort {
public:
    // Returns configured chunks and records the bounded read size.
    std::optional<std::vector<ByteVector>> read_configuration_chunks(
        std::size_t maximum_chunk_size) override {
        ++read_count;
        requested_chunk_size = maximum_chunk_size;
        return chunks;
    }

    std::optional<std::vector<ByteVector>> chunks =
        std::vector<ByteVector>{};
    std::size_t read_count = 0U;
    std::size_t requested_chunk_size = 0U;
};

}  // namespace

TEST_CASE(cfg_020_and_021_live_load_is_bounded_and_applies_chunk_rules) {
    FakeLiveConfigurationPort port;
    port.chunks = std::vector<ByteVector>{
        bytes(";ignored=value"),
        bytes("\nignored=value"),
        bytes("\rignored=value"),
        bytes("no delimiter"),
        bytes(" key = value # retained "),
    };
    LiveConfiguration live;

    live.ensure_loaded(port);

    REQUIRE_EQ(port.requested_chunk_size, 511U);
    REQUIRE_EQ(live.entry_count(), 1U);
    REQUIRE_EQ(live.find("key"),
               std::optional<std::string>("value # retained"));
}

TEST_CASE(cfg_020_live_load_truncates_keys_and_values_and_holds_only_100_entries) {
    FakeLiveConfigurationPort port;
    std::vector<ByteVector> chunks;
    for (std::size_t index = 0U; index < 101U; ++index) {
        chunks.push_back(bytes("key" + std::to_string(index) + "=value"));
    }
    chunks[0] = bytes(std::string(70U, 'k') + "=" + std::string(300U, 'v'));
    port.chunks = chunks;
    LiveConfiguration live;

    live.ensure_loaded(port);

    REQUIRE_EQ(live.entry_count(), 100U);
    REQUIRE_EQ(live.entries().front().key.size(), 63U);
    REQUIRE_EQ(live.entries().front().value.size(), 255U);
    REQUIRE(!live.find("key100").has_value());
}

TEST_CASE(cfg_022_duplicates_remain_and_lookup_and_update_use_the_first) {
    FakeLiveConfigurationPort port;
    port.chunks = std::vector<ByteVector>{bytes("same=first"), bytes("same=second")};
    LiveConfiguration live;
    live.ensure_loaded(port);

    REQUIRE_EQ(live.entry_count(), 2U);
    REQUIRE_EQ(live.find("same"), std::optional<std::string>("first"));
    REQUIRE(live.set("same", "updated"));
    REQUIRE_EQ(live.entries()[0].value, std::string("updated"));
    REQUIRE_EQ(live.entries()[1].value, std::string("second"));
}

TEST_CASE(cfg_022_missing_file_marks_the_view_loaded_until_explicit_reset) {
    FakeLiveConfigurationPort port;
    port.chunks = std::nullopt;
    LiveConfiguration live;

    live.ensure_loaded(port);
    port.chunks = std::vector<ByteVector>{bytes("appeared=value")};
    live.ensure_loaded(port);

    REQUIRE_EQ(port.read_count, 1U);
    REQUIRE(!live.find("appeared").has_value());
    live.reset();
    live.ensure_loaded(port);
    REQUIRE_EQ(port.read_count, 2U);
    REQUIRE_EQ(live.find("appeared"), std::optional<std::string>("value"));
}

TEST_CASE(cfg_023_new_live_values_truncate_but_still_report_success) {
    LiveConfiguration live;
    const std::string key(70U, 'k');
    const std::string value(300U, 'v');

    REQUIRE(live.set(key, value));

    REQUIRE_EQ(live.entries().front().key.size(), 63U);
    REQUIRE_EQ(live.entries().front().value.size(), 255U);
}

TEST_CASE(cfg_023_repeated_oversized_new_key_can_append_and_full_view_rejects) {
    LiveConfiguration live;
    const std::string oversized_key(70U, 'k');
    REQUIRE(live.set(oversized_key, "one"));
    REQUIRE(live.set(oversized_key, "two"));
    REQUIRE_EQ(live.entry_count(), 2U);

    for (std::size_t index = live.entry_count(); index < 100U; ++index) {
        REQUIRE(live.set("key" + std::to_string(index), "value"));
    }
    REQUIRE(!live.set("overflow", "value"));
    REQUIRE_EQ(live.entry_count(), 100U);
}
