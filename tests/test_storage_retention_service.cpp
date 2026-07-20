// Verifies deletion sequencing, usage refresh, failures, and persistence requests.
#include "test.hpp"
#include "firmware/application/storage_retention_service.hpp"

namespace {
class FakeRetentionPort final : public firmware::application::StorageRetentionPort {
public:
    std::optional<firmware::application::StorageUsage> read_usage() override {
        ++usage_reads;
        if (usage_reads == 1U) return firmware::application::StorageUsage{100U, 30U};
        return firmware::application::StorageUsage{100U, 40U};
    }
    std::optional<std::vector<firmware::application::RetentionCandidate>> list_video_directory() override {
        return std::vector<firmware::application::RetentionCandidate>{{"/sd/videos/a.avi", 1U, true}};
    }
    bool remove_file(std::string_view path) override {
        removed = std::string(path);
        return remove_success;
    }
    void request_persistence() override { persisted = true; }
    std::size_t usage_reads = 0U;
    bool remove_success = true;
    bool persisted = false;
    std::string removed;
};
}  // namespace

TEST_CASE(rec_021_and_022_service_refreshes_after_successful_delete) {
    FakeRetentionPort port;
    firmware::application::StorageRetentionService service;
    service.run_check(port);
    REQUIRE_EQ(port.removed, std::string("/sd/videos/a.avi"));
    REQUIRE_EQ(port.usage_reads, 2U);
}

TEST_CASE(rec_023_service_requests_persistence_on_sixtieth_check) {
    FakeRetentionPort port;
    firmware::application::StorageRetentionService service;
    for (int i = 0; i < 60; ++i) service.run_check(port);
    REQUIRE(port.persisted);
}
