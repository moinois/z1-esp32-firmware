// Verifies deletion sequencing, usage refresh, failures, and persistence requests.
#include "test.hpp"
#include "firmware/application/storage_retention_service.hpp"

namespace {
class FakeRetentionPort final : public firmware::application::StorageRetentionPort {
public:
    std::optional<firmware::application::StorageUsage> read_usage() override {
        ++usage_reads;
        if (usage_reads == 1U) {
            return initial_usage_available
                ? std::optional(initial_usage)
                : std::nullopt;
        }
        return refreshed_usage_available
            ? std::optional(refreshed_usage)
            : std::nullopt;
    }
    std::optional<std::vector<firmware::application::RetentionCandidate>> list_video_directory() override {
        return directory_available ? std::optional(candidates) : std::nullopt;
    }
    bool remove_file(std::string_view path) override {
        removed.emplace_back(path);
        return remove_success;
    }
    void request_persistence() override { persisted = true; }
    firmware::application::StorageUsage initial_usage{100U, 30U};
    firmware::application::StorageUsage refreshed_usage{100U, 40U};
    std::vector<firmware::application::RetentionCandidate> candidates{
        {"/sd/videos/a.avi", 1U, true}};
    bool initial_usage_available = true;
    bool refreshed_usage_available = true;
    bool directory_available = true;
    std::size_t usage_reads = 0U;
    bool remove_success = true;
    bool persisted = false;
    std::vector<std::string> removed;
};
}  // namespace

TEST_CASE(rec_021_and_022_service_refreshes_after_successful_delete) {
    FakeRetentionPort port;
    firmware::application::StorageRetentionService service;
    service.run_check(port);
    REQUIRE_EQ(port.removed, std::vector<std::string>({"/sd/videos/a.avi"}));
    REQUIRE_EQ(port.usage_reads, 2U);
}

TEST_CASE(rec_023_service_requests_persistence_on_sixtieth_check) {
    FakeRetentionPort port;
    firmware::application::StorageRetentionService service;
    for (int i = 0; i < 60; ++i) service.run_check(port);
    REQUIRE(port.persisted);
}

TEST_CASE(rec_024_service_stops_when_usage_or_directory_is_unavailable) {
    FakeRetentionPort missing_usage;
    missing_usage.initial_usage_available = false;
    firmware::application::StorageRetentionService usage_service;
    usage_service.run_check(missing_usage);
    REQUIRE_EQ(missing_usage.usage_reads, 1U);
    REQUIRE(missing_usage.removed.empty());

    FakeRetentionPort missing_directory;
    missing_directory.directory_available = false;
    firmware::application::StorageRetentionService directory_service;
    directory_service.run_check(missing_directory);
    REQUIRE_EQ(missing_directory.usage_reads, 1U);
    REQUIRE(missing_directory.removed.empty());
}

TEST_CASE(rec_021_delete_failure_continues_without_refresh) {
    FakeRetentionPort port;
    port.remove_success = false;
    firmware::application::StorageRetentionService service;
    service.run_check(port);

    REQUIRE_EQ(port.removed.size(), 1U);
    REQUIRE_EQ(port.usage_reads, 1U);
}

TEST_CASE(rec_022_service_stops_on_missing_or_invalid_refreshed_usage) {
    FakeRetentionPort missing;
    missing.refreshed_usage_available = false;
    firmware::application::StorageRetentionService missing_service;
    missing_service.run_check(missing);
    REQUIRE_EQ(missing.usage_reads, 2U);

    FakeRetentionPort zero;
    zero.refreshed_usage = {0U, 0U};
    firmware::application::StorageRetentionService zero_service;
    zero_service.run_check(zero);
    REQUIRE_EQ(zero.usage_reads, 2U);

    FakeRetentionPort inverted;
    inverted.refreshed_usage = {100U, 101U};
    firmware::application::StorageRetentionService inverted_service;
    inverted_service.run_check(inverted);
    REQUIRE_EQ(inverted.usage_reads, 2U);
}
