#pragma once

#include "fastseed/crawler/candidate.hpp"
#include "fastseed/health/peer_health.hpp"
#include "fastseed/crawler/adaptive_feedback.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <stdexcept>
#include <vector>
#include <utility>

struct sqlite3;

namespace fastseed::db {

enum class DatabaseErrorCode {
    OpenFailed,
    SchemaFailed,
    QueryFailed,
    TransactionFailed,
    IntegrityFailed,
    LockFailed
};

class DatabaseError : public std::runtime_error {
public:
    DatabaseError(DatabaseErrorCode code, const std::string& message);
    DatabaseErrorCode code() const noexcept { return code_; }
private:
    DatabaseErrorCode code_;
};

struct DatabaseConfig {
    std::string path{"fastseed.sqlite3"};
    std::size_t max_peers{250000};
    std::chrono::hours stale_after{24 * 30};
    std::chrono::hours failed_after{24 * 14};
    int busy_timeout_ms{2500};
};

struct SchedulerStateRecord {
    transport::TransportKind transport{};
    crawler::PersistedTransportState state{};
};

struct DatabaseStats {
    std::uint64_t peers{};
    std::uint64_t hot{};
    std::uint64_t warm{};
    std::uint64_t cold{};
    std::uint64_t quarantined{};
    std::uint64_t bytes{};
};

class PeerDatabase {
public:
    explicit PeerDatabase(DatabaseConfig config = {});
    ~PeerDatabase();
    PeerDatabase(const PeerDatabase&) = delete;
    PeerDatabase& operator=(const PeerDatabase&) = delete;

    void open();
    void close() noexcept;
    bool is_open() const noexcept;

    void save(const health::PeerHealth& peer);
    void save_batch(const std::vector<health::PeerHealth>& peers);

    // Persistent state is deliberately converted to a fresh candidate state.
    // A peer loaded at startup must be probed again before becoming DNS eligible.
    std::vector<crawler::PeerCandidate> load_candidates(std::size_t limit = 10000) const;
    std::vector<health::PeerHealth> load_health(std::size_t limit = 10000) const;

    DatabaseStats stats() const;
    bool integrity_check() const;
    void vacuum();
    std::size_t cleanup();
    std::size_t migrate();

    void save_scheduler_state(const crawler::PersistedTransportState& state, transport::TransportKind kind);
    std::vector<SchedulerStateRecord> load_scheduler_state() const;
    bool scheduler_state_fresh(std::int64_t updated_at_ms, std::int64_t max_age_ms) const noexcept;

    const DatabaseConfig& config() const noexcept { return config_; }

private:
    void require_open() const;
    void configure();
    void create_schema();
    void exec(const char* sql) const;
    static std::int64_t to_ms(std::chrono::system_clock::time_point value);
    static std::chrono::system_clock::time_point from_ms(std::int64_t value);
    static const char* state_name(health::PeerState state) noexcept;
    static health::PeerState state_from_name(const unsigned char* value) noexcept;
    static const char* source_name(crawler::CandidateSource source) noexcept;
    static crawler::CandidateSource source_from_name(const unsigned char* value) noexcept;

    DatabaseConfig config_;
    sqlite3* db_{nullptr};
    int lock_fd_{-1};
    std::string lock_path_;
    mutable std::mutex mutex_;
};

} // namespace fastseed::db
