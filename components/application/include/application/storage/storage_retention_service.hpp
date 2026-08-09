/** @file @brief Declares the storage retention service over replaceable filesystem operations. */
#pragma once

#include "application/storage/storage_retention.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace firmware::application {

/** Capacity snapshot used to decide whether recording retention is required. */
struct StorageUsage {
    std::uint64_t total_bytes = 0U;
    std::uint64_t free_bytes = 0U;
};

/** Filesystem operations required by the storage-retention policy. */
class StorageRetentionPort {
public:
    virtual ~StorageRetentionPort() = default;
    virtual std::optional<StorageUsage> read_usage() = 0;
    virtual std::optional<std::vector<RetentionCandidate>> list_video_directory() = 0;
    virtual bool remove_file(std::string_view path) = 0;
    virtual void request_persistence() = 0;
};

/// Runs one check, pruning candidates and refreshing usage after each deletion.
class StorageRetentionService {
public:
    void run_check(StorageRetentionPort& port);

private:
    std::uint32_t check_number_ = 0U;
};

}  // namespace firmware::application
