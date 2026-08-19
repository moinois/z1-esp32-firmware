// Verifies the bounded persistent-store recovery sequence before target startup.
#include "test.hpp"

#include "application/runtime/persistent_store_initialization.hpp"

#include <cstddef>
#include <string>
#include <vector>

using firmware::application::PersistentStoreInitializationPort;
using firmware::application::PersistentStoreInitializationResult;
using firmware::application::initialize_persistent_store;

namespace {

class FakePersistentStorePort final : public PersistentStoreInitializationPort {
public:
    PersistentStoreInitializationResult initialize() override {
        calls.emplace_back("initialize");
        return initialization_results.at(initialization_index++);
    }
    bool erase() override {
        calls.emplace_back("erase");
        return erase_results.at(erase_index++);
    }
    void report_exhausted_recovery() override {
        calls.emplace_back("exhausted");
    }
    void report_general_recovery() override { calls.emplace_back("general"); }

    std::vector<PersistentStoreInitializationResult> initialization_results;
    std::vector<bool> erase_results;
    std::size_t initialization_index = 0U;
    std::size_t erase_index = 0U;
    std::vector<std::string> calls;
};

}  // namespace

TEST_CASE(boot_002_exhausted_store_uses_two_bounded_recovery_rounds) {
    FakePersistentStorePort port;
    port.initialization_results = {
        PersistentStoreInitializationResult::exhausted_pages,
        PersistentStoreInitializationResult::other_failure,
        PersistentStoreInitializationResult::success};
    port.erase_results = {true, true};

    REQUIRE(initialize_persistent_store(port));
    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"initialize", "exhausted", "erase",
                                         "initialize", "general", "erase",
                                         "initialize"}));
}

TEST_CASE(boot_002_first_recovery_erase_failure_is_fatal) {
    FakePersistentStorePort port;
    port.initialization_results = {
        PersistentStoreInitializationResult::incompatible_version};
    port.erase_results = {false};

    REQUIRE(!initialize_persistent_store(port));
    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"initialize", "exhausted", "erase"}));
}

TEST_CASE(boot_003_final_initialization_runs_after_failed_erase) {
    FakePersistentStorePort port;
    port.initialization_results = {
        PersistentStoreInitializationResult::other_failure,
        PersistentStoreInitializationResult::success};
    port.erase_results = {false};

    REQUIRE(initialize_persistent_store(port));
    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"initialize", "general", "erase",
                                         "initialize"}));
}
