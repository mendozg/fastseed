#include "fastseed/db/peer_database.hpp"
#include <sqlite3.h>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace fastseed::db {
namespace {

std::string sqlite_message(sqlite3* db, const char* fallback) {
    const char* msg = db ? sqlite3_errmsg(db) : nullptr;
    return msg ? msg : fallback;
}

void check_rc(sqlite3* db, int rc, DatabaseErrorCode code, const char* what) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW)
        throw DatabaseError(code, std::string(what) + ": " + sqlite_message(db, what));
}

class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) {
        check_rc(db_, sqlite3_prepare_v2(db_, sql, -1, &stmt_, nullptr),
                 DatabaseErrorCode::QueryFailed, "sqlite prepare failed");
    }
    ~Statement() { if (stmt_) sqlite3_finalize(stmt_); }
    sqlite3_stmt* get() { return stmt_; }
private:
    sqlite3* db_;
    sqlite3_stmt* stmt_{nullptr};
};

void bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
    if (sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
        throw DatabaseError(DatabaseErrorCode::QueryFailed, "sqlite bind text failed");
}
void bind_i64(sqlite3_stmt* stmt, int index, std::int64_t value) {
    if (sqlite3_bind_int64(stmt, index, value) != SQLITE_OK)
        throw DatabaseError(DatabaseErrorCode::QueryFailed, "sqlite bind integer failed");
}

std::string hex_encode(const std::vector<std::uint8_t>& bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out; out.reserve(bytes.size() * 2);
    for (auto b : bytes) { out.push_back(digits[b >> 4]); out.push_back(digits[b & 0x0f]); }
    return out;
}

std::vector<std::uint8_t> hex_decode(const unsigned char* text) {
    std::vector<std::uint8_t> out;
    if (!text) return out;
    const std::string s(reinterpret_cast<const char*>(text));
    if (s.size() % 2 != 0) return out;
    out.reserve(s.size() / 2);
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < s.size(); i += 2) {
        int hi = nibble(s[i]), lo = nibble(s[i+1]);
        if (hi < 0 || lo < 0) { out.clear(); return out; }
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return out;
}

int network_value(crawler::CandidateNetwork network) { return static_cast<int>(network); }

} // namespace

DatabaseError::DatabaseError(DatabaseErrorCode code, const std::string& message)
    : std::runtime_error(message), code_(code) {}

PeerDatabase::PeerDatabase(DatabaseConfig config) : config_(std::move(config)) {
    if (config_.max_peers == 0) config_.max_peers = 1;
}

PeerDatabase::~PeerDatabase() { close(); }

void PeerDatabase::open() {
    std::lock_guard lock(mutex_);
    if (db_) return;
    try {
        const auto parent = std::filesystem::path(config_.path).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
    } catch (const std::exception& e) {
        throw DatabaseError(DatabaseErrorCode::OpenFailed, std::string("cannot create database directory: ") + e.what());
    }
    sqlite3* handle = nullptr;
    const int rc = sqlite3_open_v2(config_.path.c_str(), &handle,
                                   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        const std::string msg = sqlite_message(handle, "sqlite open failed");
        if (handle) sqlite3_close(handle);
        throw DatabaseError(DatabaseErrorCode::OpenFailed, msg);
    }
    db_ = handle;
    try {
        lock_path_ = config_.path + ".lock";
        lock_fd_ = ::open(lock_path_.c_str(), O_RDWR | O_CREAT, 0600);
        if (lock_fd_ < 0 || ::flock(lock_fd_, LOCK_EX | LOCK_NB) != 0)
            throw DatabaseError(DatabaseErrorCode::LockFailed, "database lock is already held by another FastSeed instance");
        configure();
        create_schema();
    } catch (...) {
        if (lock_fd_ >= 0) { ::flock(lock_fd_, LOCK_UN); ::close(lock_fd_); lock_fd_ = -1; }
        sqlite3_close(db_); db_ = nullptr; throw;
    }
}

void PeerDatabase::close() noexcept {
    std::lock_guard lock(mutex_);
    if (db_) { sqlite3_close(db_); db_ = nullptr; }
    if (lock_fd_ >= 0) { ::flock(lock_fd_, LOCK_UN); ::close(lock_fd_); lock_fd_ = -1; }
}

bool PeerDatabase::is_open() const noexcept {
    std::lock_guard lock(mutex_);
    return db_ != nullptr;
}

void PeerDatabase::require_open() const {
    if (!db_) throw DatabaseError(DatabaseErrorCode::OpenFailed, "database is not open");
}

void PeerDatabase::exec(const char* sql) const {
    char* error = nullptr;
    const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        std::string message = error ? error : sqlite_message(db_, "sqlite exec failed");
        sqlite3_free(error);
        throw DatabaseError(DatabaseErrorCode::QueryFailed, message);
    }
}

void PeerDatabase::configure() {
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
    exec("PRAGMA foreign_keys=ON;");
    std::ostringstream oss;
    oss << "PRAGMA busy_timeout=" << config_.busy_timeout_ms << ";";
    exec(oss.str().c_str());
}

void PeerDatabase::create_schema() {
    exec(R"SQL(
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER NOT NULL
);
INSERT INTO schema_version(version)
SELECT 1 WHERE NOT EXISTS (SELECT 1 FROM schema_version);
CREATE TABLE IF NOT EXISTS peers (
    id INTEGER PRIMARY KEY,
    address TEXT NOT NULL,
    address_bytes TEXT NOT NULL DEFAULT '',
    port INTEGER NOT NULL CHECK(port BETWEEN 1 AND 65535),
    network INTEGER NOT NULL CHECK(network IN (1,2,4,5)),
    transport_kind INTEGER NOT NULL DEFAULT 0 CHECK(transport_kind IN (0,1,2)),
    protocol_version INTEGER NOT NULL DEFAULT 0,
    services INTEGER NOT NULL DEFAULT 0,
    user_agent TEXT NOT NULL DEFAULT '',
    state TEXT NOT NULL,
    source TEXT NOT NULL,
    first_seen INTEGER NOT NULL,
    last_seen INTEGER NOT NULL,
    last_success INTEGER NOT NULL DEFAULT 0,
    last_failure INTEGER NOT NULL DEFAULT 0,
    successful_probes INTEGER NOT NULL DEFAULT 0,
    failed_probes INTEGER NOT NULL DEFAULT 0,
    consecutive_successes INTEGER NOT NULL DEFAULT 0,
    consecutive_failures INTEGER NOT NULL DEFAULT 0,
    transport_attempts INTEGER NOT NULL DEFAULT 0,
    transport_successes INTEGER NOT NULL DEFAULT 0,
    transport_failures INTEGER NOT NULL DEFAULT 0,
    observations INTEGER NOT NULL DEFAULT 0,
    latency_last_ms INTEGER NOT NULL DEFAULT 0,
    latency_avg_ms INTEGER NOT NULL DEFAULT 0,
    latency_p95_ms INTEGER NOT NULL DEFAULT 0,
    latency_history TEXT NOT NULL DEFAULT '',
    next_probe INTEGER NOT NULL DEFAULT 0,
    quarantine_until INTEGER NOT NULL DEFAULT 0,
    ipv4_prefix TEXT NOT NULL DEFAULT '',
    ipv6_prefix TEXT NOT NULL DEFAULT '',
    asn INTEGER NOT NULL DEFAULT 0,
    country_code TEXT NOT NULL DEFAULT '',
    last_error TEXT NOT NULL DEFAULT '',
    transport_last_error TEXT NOT NULL DEFAULT '',
    UNIQUE(address, port)
);
CREATE INDEX IF NOT EXISTS idx_peers_last_success ON peers(last_success);
CREATE INDEX IF NOT EXISTS idx_peers_state ON peers(state);
CREATE INDEX IF NOT EXISTS idx_peers_last_failure ON peers(last_failure);
CREATE INDEX IF NOT EXISTS idx_peers_network ON peers(network);
CREATE INDEX IF NOT EXISTS idx_peers_source ON peers(source);
CREATE INDEX IF NOT EXISTS idx_peers_next_probe ON peers(next_probe);
CREATE TABLE IF NOT EXISTS scheduler_state (
    transport INTEGER PRIMARY KEY CHECK(transport IN (0,1,2)), state_version INTEGER NOT NULL, attempts INTEGER NOT NULL, successes INTEGER NOT NULL, failures INTEGER NOT NULL,
    timeouts INTEGER NOT NULL, handshake_failures INTEGER NOT NULL, unsupported INTEGER NOT NULL, consecutive_failures INTEGER NOT NULL, consecutive_successes INTEGER NOT NULL,
    latency_avg_ms INTEGER NOT NULL, learned_capacity INTEGER NOT NULL, updated_at_ms INTEGER NOT NULL
);
)SQL");
    Statement version_stmt(db_, "SELECT MAX(version) FROM schema_version;");
    int version = 0;
    if (sqlite3_step(version_stmt.get()) == SQLITE_ROW) version = sqlite3_column_int(version_stmt.get(), 0);
    if (version < 2) {
        Statement col_stmt(db_, "SELECT COUNT(*) FROM pragma_table_info('peers') WHERE name='address_bytes';");
        int has_column = 0;
        if (sqlite3_step(col_stmt.get()) == SQLITE_ROW) has_column = sqlite3_column_int(col_stmt.get(), 0);
        if (!has_column) exec("ALTER TABLE peers ADD COLUMN address_bytes TEXT NOT NULL DEFAULT ''; ");
        exec("UPDATE schema_version SET version=2;");
    }
    if (version < 3) {
        auto add_column = [this](const char* name, const char* ddl) {
            Statement col_stmt(db_, (std::string("SELECT COUNT(*) FROM pragma_table_info('peers') WHERE name='") + name + "';").c_str());
            int has_column = 0;
            if (sqlite3_step(col_stmt.get()) == SQLITE_ROW) has_column = sqlite3_column_int(col_stmt.get(), 0);
            if (!has_column) exec(ddl);
        };
        add_column("transport_kind", "ALTER TABLE peers ADD COLUMN transport_kind INTEGER NOT NULL DEFAULT 0;");
        add_column("transport_attempts", "ALTER TABLE peers ADD COLUMN transport_attempts INTEGER NOT NULL DEFAULT 0;");
        add_column("transport_successes", "ALTER TABLE peers ADD COLUMN transport_successes INTEGER NOT NULL DEFAULT 0;");
        add_column("transport_failures", "ALTER TABLE peers ADD COLUMN transport_failures INTEGER NOT NULL DEFAULT 0;");
        add_column("transport_last_error", "ALTER TABLE peers ADD COLUMN transport_last_error TEXT NOT NULL DEFAULT ''; ");
        exec("UPDATE peers SET transport_kind=CASE network WHEN 4 THEN 1 WHEN 5 THEN 2 ELSE 0 END WHERE transport_kind=0;");
        exec("UPDATE schema_version SET version=3;");
    }
    // scheduler_state is created above for both new and upgraded databases.
    if (version < 4) exec("UPDATE schema_version SET version=4;");
}

std::int64_t PeerDatabase::to_ms(std::chrono::system_clock::time_point value) {
    if (value.time_since_epoch().count() == 0) return 0;
    return std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch()).count();
}
std::chrono::system_clock::time_point PeerDatabase::from_ms(std::int64_t value) {
    if (value == 0) return {};
    return std::chrono::system_clock::time_point{std::chrono::milliseconds(value)};
}

const char* PeerDatabase::state_name(health::PeerState state) noexcept { return health::to_string(state).data(); }
const char* PeerDatabase::source_name(crawler::CandidateSource source) noexcept { return crawler::to_string(source).data(); }

health::PeerState PeerDatabase::state_from_name(const unsigned char* value) noexcept {
    if (!value) return health::PeerState::Cold;
    const std::string s(reinterpret_cast<const char*>(value));
    if (s == "HOT") return health::PeerState::Hot;
    if (s == "WARM") return health::PeerState::Warm;
    if (s == "VALIDATED") return health::PeerState::Validated;
    if (s == "QUARANTINED") return health::PeerState::Quarantined;
    if (s == "COOLDOWN") return health::PeerState::Cooldown;
    if (s == "CANDIDATE") return health::PeerState::Candidate;
    if (s == "PROBING") return health::PeerState::Probing;
    if (s == "DISCOVERED") return health::PeerState::Discovered;
    return health::PeerState::Cold;
}

crawler::CandidateSource PeerDatabase::source_from_name(const unsigned char* value) noexcept {
    if (!value) return crawler::CandidateSource::PersistentDatabase;
    const std::string s(reinterpret_cast<const char*>(value));
    if (s == "bootstrap") return crawler::CandidateSource::Bootstrap;
    if (s == "addr") return crawler::CandidateSource::Addr;
    if (s == "addrv2") return crawler::CandidateSource::AddrV2;
    if (s == "manual") return crawler::CandidateSource::Manual;
    if (s == "peer_exchange") return crawler::CandidateSource::PeerExchange;
    return crawler::CandidateSource::PersistentDatabase;
}

static int transport_value(fastseed::transport::TransportKind kind) {
    switch (kind) {
    case fastseed::transport::TransportKind::TorSocks5: return 1;
    case fastseed::transport::TransportKind::I2pSam: return 2;
    case fastseed::transport::TransportKind::DirectTcp: return 0;
    }
    return 0;
}

static fastseed::transport::TransportKind transport_from_value(int value) {
    if (value == 1) return fastseed::transport::TransportKind::TorSocks5;
    if (value == 2) return fastseed::transport::TransportKind::I2pSam;
    return fastseed::transport::TransportKind::DirectTcp;
}

void PeerDatabase::save(const health::PeerHealth& p) {
    std::lock_guard lock(mutex_);
    require_open();
    Statement st(db_, R"SQL(
INSERT INTO peers(address,address_bytes,port,network,transport_kind,protocol_version,services,user_agent,state,source,
 first_seen,last_seen,last_success,last_failure,successful_probes,failed_probes,
 consecutive_successes,consecutive_failures,transport_attempts,transport_successes,transport_failures,observations,latency_last_ms,latency_avg_ms,
 latency_p95_ms,latency_history,next_probe,quarantine_until,last_error,transport_last_error)
VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
ON CONFLICT(address,port) DO UPDATE SET
 address_bytes=excluded.address_bytes, network=excluded.network, transport_kind=excluded.transport_kind, protocol_version=excluded.protocol_version, services=excluded.services,
 user_agent=excluded.user_agent, state=excluded.state, source=excluded.source,
 first_seen=excluded.first_seen, last_seen=excluded.last_seen, last_success=excluded.last_success,
 last_failure=excluded.last_failure, successful_probes=excluded.successful_probes,
 failed_probes=excluded.failed_probes, consecutive_successes=excluded.consecutive_successes,
 consecutive_failures=excluded.consecutive_failures, transport_attempts=excluded.transport_attempts, transport_successes=excluded.transport_successes, transport_failures=excluded.transport_failures, observations=excluded.observations,
 latency_last_ms=excluded.latency_last_ms, latency_avg_ms=excluded.latency_avg_ms,
 latency_p95_ms=excluded.latency_p95_ms, latency_history=excluded.latency_history, next_probe=excluded.next_probe,
 quarantine_until=excluded.quarantine_until, last_error=excluded.last_error, transport_last_error=excluded.transport_last_error
)SQL");
    int i = 1;
    bind_text(st.get(), i++, p.host);
    bind_text(st.get(), i++, hex_encode(p.address_bytes));
    bind_i64(st.get(), i++, p.port);
    bind_i64(st.get(), i++, p.network == crawler::CandidateNetwork::Unknown ? (p.host.find(':') != std::string::npos ? 2 : 1) : network_value(p.network));
    bind_i64(st.get(), i++, transport_value(p.transport));
    bind_i64(st.get(), i++, p.protocol_version);
    bind_i64(st.get(), i++, static_cast<std::int64_t>(p.services));
    bind_text(st.get(), i++, p.user_agent);
    bind_text(st.get(), i++, state_name(p.state));
    bind_text(st.get(), i++, source_name(p.source));
    bind_i64(st.get(), i++, to_ms(p.first_seen));
    bind_i64(st.get(), i++, to_ms(p.last_seen));
    bind_i64(st.get(), i++, to_ms(p.last_success));
    bind_i64(st.get(), i++, to_ms(p.last_failure));
    bind_i64(st.get(), i++, p.successful_probes);
    bind_i64(st.get(), i++, p.failed_probes);
    bind_i64(st.get(), i++, p.consecutive_successes);
    bind_i64(st.get(), i++, p.consecutive_failures);
    bind_i64(st.get(), i++, p.transport_attempts);
    bind_i64(st.get(), i++, p.transport_successes);
    bind_i64(st.get(), i++, p.transport_failures);
    bind_i64(st.get(), i++, p.observations);
    bind_i64(st.get(), i++, p.latency_last.count());
    bind_i64(st.get(), i++, p.latency_avg.count());
    bind_i64(st.get(), i++, p.latency_p95.count());
    std::string history;
    for (std::size_t n = 0; n < p.latency_history.size(); ++n) {
        if (n) history += ',';
        history += std::to_string(p.latency_history[n].count());
    }
    bind_text(st.get(), i++, history);
    bind_i64(st.get(), i++, to_ms(p.next_probe));
    bind_i64(st.get(), i++, to_ms(p.quarantine_until));
    bind_text(st.get(), i++, p.last_error);
    bind_text(st.get(), i++, p.transport_last_error);
    check_rc(db_, sqlite3_step(st.get()), DatabaseErrorCode::QueryFailed, "sqlite save failed");
    std::ostringstream limit_sql;
    limit_sql << "DELETE FROM peers WHERE id NOT IN (SELECT id FROM peers ORDER BY last_seen DESC LIMIT " << config_.max_peers << ");";
    exec(limit_sql.str().c_str());
}

void PeerDatabase::save_batch(const std::vector<health::PeerHealth>& peers) {
    std::lock_guard lock(mutex_);
    require_open();
    exec("BEGIN IMMEDIATE;");
    try {
        Statement st(db_, R"SQL(
INSERT INTO peers(address,address_bytes,port,network,transport_kind,protocol_version,services,user_agent,state,source,
 first_seen,last_seen,last_success,last_failure,successful_probes,failed_probes,
 consecutive_successes,consecutive_failures,transport_attempts,transport_successes,transport_failures,observations,latency_last_ms,latency_avg_ms,
 latency_p95_ms,latency_history,next_probe,quarantine_until,last_error,transport_last_error)
VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
ON CONFLICT(address,port) DO UPDATE SET
 address_bytes=excluded.address_bytes, network=excluded.network, transport_kind=excluded.transport_kind, protocol_version=excluded.protocol_version, services=excluded.services,
 user_agent=excluded.user_agent, state=excluded.state, source=excluded.source,
 first_seen=excluded.first_seen, last_seen=excluded.last_seen, last_success=excluded.last_success,
 last_failure=excluded.last_failure, successful_probes=excluded.successful_probes,
 failed_probes=excluded.failed_probes, consecutive_successes=excluded.consecutive_successes,
 consecutive_failures=excluded.consecutive_failures, transport_attempts=excluded.transport_attempts, transport_successes=excluded.transport_successes, transport_failures=excluded.transport_failures, observations=excluded.observations,
 latency_last_ms=excluded.latency_last_ms, latency_avg_ms=excluded.latency_avg_ms,
 latency_p95_ms=excluded.latency_p95_ms, latency_history=excluded.latency_history, next_probe=excluded.next_probe,
 quarantine_until=excluded.quarantine_until, last_error=excluded.last_error, transport_last_error=excluded.transport_last_error
)SQL");
        for (const auto& p : peers) {
            sqlite3_reset(st.get()); sqlite3_clear_bindings(st.get());
            int i=1;
            bind_text(st.get(),i++,p.host); bind_text(st.get(),i++,hex_encode(p.address_bytes)); bind_i64(st.get(),i++,p.port);
            bind_i64(st.get(),i++,p.network == crawler::CandidateNetwork::Unknown ? (p.host.find(':') != std::string::npos ? 2 : 1) : network_value(p.network));
            bind_i64(st.get(),i++,transport_value(p.transport));
            bind_i64(st.get(),i++,p.protocol_version); bind_i64(st.get(),i++,static_cast<std::int64_t>(p.services));
            bind_text(st.get(),i++,p.user_agent); bind_text(st.get(),i++,state_name(p.state)); bind_text(st.get(),i++,source_name(p.source));
            bind_i64(st.get(),i++,to_ms(p.first_seen)); bind_i64(st.get(),i++,to_ms(p.last_seen)); bind_i64(st.get(),i++,to_ms(p.last_success)); bind_i64(st.get(),i++,to_ms(p.last_failure));
            bind_i64(st.get(),i++,p.successful_probes); bind_i64(st.get(),i++,p.failed_probes); bind_i64(st.get(),i++,p.consecutive_successes); bind_i64(st.get(),i++,p.consecutive_failures); bind_i64(st.get(),i++,p.transport_attempts); bind_i64(st.get(),i++,p.transport_successes); bind_i64(st.get(),i++,p.transport_failures); bind_i64(st.get(),i++,p.observations);
            bind_i64(st.get(),i++,p.latency_last.count()); bind_i64(st.get(),i++,p.latency_avg.count()); bind_i64(st.get(),i++,p.latency_p95.count());
            std::string history; for (std::size_t n=0;n<p.latency_history.size();++n){ if(n)history+=','; history+=std::to_string(p.latency_history[n].count()); }
            bind_text(st.get(),i++,history); bind_i64(st.get(),i++,to_ms(p.next_probe)); bind_i64(st.get(),i++,to_ms(p.quarantine_until)); bind_text(st.get(),i++,p.last_error); bind_text(st.get(),i++,p.transport_last_error);
            check_rc(db_,sqlite3_step(st.get()),DatabaseErrorCode::TransactionFailed,"sqlite batch save failed");
        }
        std::ostringstream limit_sql;
        limit_sql << "DELETE FROM peers WHERE id NOT IN (SELECT id FROM peers ORDER BY last_seen DESC LIMIT " << config_.max_peers << ");";
        exec(limit_sql.str().c_str());
        exec("COMMIT;");
    } catch (...) {
        try { exec("ROLLBACK;"); } catch (...) {}
        throw;
    }
}

std::vector<crawler::PeerCandidate> PeerDatabase::load_candidates(std::size_t limit) const {
    std::lock_guard lock(mutex_); require_open();
    Statement st(db_, "SELECT address,address_bytes,port,source,network FROM peers ORDER BY last_success DESC,last_seen DESC LIMIT ?;");
    bind_i64(st.get(),1,static_cast<std::int64_t>(limit));
    std::vector<crawler::PeerCandidate> out;
    while (sqlite3_step(st.get()) == SQLITE_ROW) {
        crawler::PeerCandidate p;
        p.host = reinterpret_cast<const char*>(sqlite3_column_text(st.get(),0));
        p.address_bytes = hex_decode(sqlite3_column_text(st.get(),1));
        p.port = static_cast<std::uint16_t>(sqlite3_column_int(st.get(),2));
        p.source = crawler::CandidateSource::PersistentDatabase;
        p.network = static_cast<crawler::CandidateNetwork>(sqlite3_column_int(st.get(),4));
        p.state = p.transport_probe_supported() ? crawler::CandidateState::Queued : crawler::CandidateState::AwaitingTransport;
        out.push_back(std::move(p));
    }
    return out;
}

std::vector<health::PeerHealth> PeerDatabase::load_health(std::size_t limit) const {
    std::lock_guard lock(mutex_); require_open();
    Statement st(db_, "SELECT address,address_bytes,port,network,transport_kind,protocol_version,services,user_agent,state,source,first_seen,last_seen,last_success,last_failure,successful_probes,failed_probes,consecutive_successes,consecutive_failures,transport_attempts,transport_successes,transport_failures,observations,latency_last_ms,latency_avg_ms,latency_p95_ms,latency_history,next_probe,quarantine_until,last_error,transport_last_error FROM peers ORDER BY last_seen DESC LIMIT ?;");
    bind_i64(st.get(),1,static_cast<std::int64_t>(limit));
    std::vector<health::PeerHealth> out;
    while (sqlite3_step(st.get()) == SQLITE_ROW) {
        health::PeerHealth p;
        p.host = reinterpret_cast<const char*>(sqlite3_column_text(st.get(),0));
        p.address_bytes = hex_decode(sqlite3_column_text(st.get(),1));
        p.port = static_cast<std::uint16_t>(sqlite3_column_int(st.get(),2));
        p.network = static_cast<crawler::CandidateNetwork>(sqlite3_column_int(st.get(),3));
        p.transport = transport_from_value(sqlite3_column_int(st.get(),4));
        p.protocol_version = sqlite3_column_int(st.get(),5);
        p.services = static_cast<std::uint64_t>(sqlite3_column_int64(st.get(),6));
        p.user_agent = reinterpret_cast<const char*>(sqlite3_column_text(st.get(),7));
        p.state = state_from_name(sqlite3_column_text(st.get(),8));
        p.source = source_from_name(sqlite3_column_text(st.get(),9));
        p.first_seen=from_ms(sqlite3_column_int64(st.get(),10)); p.last_seen=from_ms(sqlite3_column_int64(st.get(),11)); p.last_success=from_ms(sqlite3_column_int64(st.get(),12)); p.last_failure=from_ms(sqlite3_column_int64(st.get(),13));
        p.successful_probes=sqlite3_column_int64(st.get(),14); p.failed_probes=sqlite3_column_int64(st.get(),15); p.consecutive_successes=sqlite3_column_int(st.get(),16); p.consecutive_failures=sqlite3_column_int(st.get(),17);
        p.transport_attempts=sqlite3_column_int64(st.get(),18); p.transport_successes=sqlite3_column_int64(st.get(),19); p.transport_failures=sqlite3_column_int64(st.get(),20); p.observations=sqlite3_column_int64(st.get(),21);
        p.latency_last=std::chrono::milliseconds(sqlite3_column_int64(st.get(),22)); p.latency_avg=std::chrono::milliseconds(sqlite3_column_int64(st.get(),23)); p.latency_p95=std::chrono::milliseconds(sqlite3_column_int64(st.get(),24));
        const char* hist=reinterpret_cast<const char*>(sqlite3_column_text(st.get(),25));
        if(hist){ std::stringstream ss(hist); std::string token; while(std::getline(ss,token,',')){ try { p.latency_history.emplace_back(std::stoll(token)); } catch(...) {} } }
        p.next_probe=from_ms(sqlite3_column_int64(st.get(),26));
        p.quarantine_until=from_ms(sqlite3_column_int64(st.get(),27));
        const char* err=reinterpret_cast<const char*>(sqlite3_column_text(st.get(),28)); p.last_error=err ? err : "";
        const char* terr=reinterpret_cast<const char*>(sqlite3_column_text(st.get(),29)); p.transport_last_error=terr ? terr : "";
        out.push_back(std::move(p));
    }
    return out;
}

DatabaseStats PeerDatabase::stats() const {
    std::lock_guard lock(mutex_); require_open(); DatabaseStats s{};
    Statement st(db_, "SELECT COUNT(*), SUM(state='HOT'), SUM(state='WARM'), SUM(state='COLD'), SUM(state='QUARANTINED') FROM peers;");
    if (sqlite3_step(st.get()) == SQLITE_ROW) { s.peers=sqlite3_column_int64(st.get(),0); s.hot=sqlite3_column_int64(st.get(),1); s.warm=sqlite3_column_int64(st.get(),2); s.cold=sqlite3_column_int64(st.get(),3); s.quarantined=sqlite3_column_int64(st.get(),4); }
    std::error_code ec; s.bytes=std::filesystem::file_size(config_.path,ec); if(ec)s.bytes=0;
    return s;
}

bool PeerDatabase::integrity_check() const {
    std::lock_guard lock(mutex_); require_open();
    Statement st(db_, "PRAGMA integrity_check;");
    if (sqlite3_step(st.get()) != SQLITE_ROW) return false;
    const auto* text=sqlite3_column_text(st.get(),0);
    return text && std::string(reinterpret_cast<const char*>(text)) == "ok";
}

void PeerDatabase::vacuum() { std::lock_guard lock(mutex_); require_open(); exec("VACUUM;"); }

std::size_t PeerDatabase::cleanup() {
    std::lock_guard lock(mutex_); require_open();
    const auto now=to_ms(std::chrono::system_clock::now());
    const auto stale=now-std::chrono::duration_cast<std::chrono::milliseconds>(config_.stale_after).count();
    const auto failed=now-std::chrono::duration_cast<std::chrono::milliseconds>(config_.failed_after).count();
    Statement st(db_, "DELETE FROM peers WHERE (last_success=0 AND last_failure>0 AND last_failure<?) OR (last_success>0 AND last_success<?);");
    bind_i64(st.get(),1,failed); bind_i64(st.get(),2,stale); check_rc(db_,sqlite3_step(st.get()),DatabaseErrorCode::QueryFailed,"sqlite cleanup failed");
    return static_cast<std::size_t>(sqlite3_changes(db_));
}

std::size_t PeerDatabase::migrate() {
    std::lock_guard lock(mutex_); require_open();
    Statement st(db_, "SELECT MAX(version) FROM schema_version;");
    int version=0; if(sqlite3_step(st.get())==SQLITE_ROW) version=sqlite3_column_int(st.get(),0);
    if(version < 1) { exec("DELETE FROM schema_version; INSERT INTO schema_version(version) VALUES(1);"); return 1; }
    if(version < 2) { exec("ALTER TABLE peers ADD COLUMN address_bytes TEXT NOT NULL DEFAULT ''; UPDATE schema_version SET version=2;"); return 2; }
    if(version < 3) { exec("ALTER TABLE peers ADD COLUMN transport_kind INTEGER NOT NULL DEFAULT 0; ALTER TABLE peers ADD COLUMN transport_attempts INTEGER NOT NULL DEFAULT 0; ALTER TABLE peers ADD COLUMN transport_successes INTEGER NOT NULL DEFAULT 0; ALTER TABLE peers ADD COLUMN transport_failures INTEGER NOT NULL DEFAULT 0; ALTER TABLE peers ADD COLUMN transport_last_error TEXT NOT NULL DEFAULT ''; UPDATE peers SET transport_kind=CASE network WHEN 4 THEN 1 WHEN 5 THEN 2 ELSE 0 END; UPDATE schema_version SET version=3;"); return 3; }
    if(version < 4) { exec("CREATE TABLE IF NOT EXISTS scheduler_state (transport INTEGER PRIMARY KEY CHECK(transport IN (0,1,2)), state_version INTEGER NOT NULL, attempts INTEGER NOT NULL, successes INTEGER NOT NULL, failures INTEGER NOT NULL, timeouts INTEGER NOT NULL, handshake_failures INTEGER NOT NULL, unsupported INTEGER NOT NULL, consecutive_failures INTEGER NOT NULL, consecutive_successes INTEGER NOT NULL, latency_avg_ms INTEGER NOT NULL, learned_capacity INTEGER NOT NULL, updated_at_ms INTEGER NOT NULL); UPDATE schema_version SET version=4;"); return 4; }
    return 0;
}

void PeerDatabase::save_scheduler_state(const crawler::PersistedTransportState& x, transport::TransportKind kind) {
 std::lock_guard lock(mutex_); require_open();
 Statement st(db_, "INSERT INTO scheduler_state(transport,state_version,attempts,successes,failures,timeouts,handshake_failures,unsupported,consecutive_failures,consecutive_successes,latency_avg_ms,learned_capacity,updated_at_ms) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(transport) DO UPDATE SET state_version=excluded.state_version,attempts=excluded.attempts,successes=excluded.successes,failures=excluded.failures,timeouts=excluded.timeouts,handshake_failures=excluded.handshake_failures,unsupported=excluded.unsupported,consecutive_failures=excluded.consecutive_failures,consecutive_successes=excluded.consecutive_successes,latency_avg_ms=excluded.latency_avg_ms,learned_capacity=excluded.learned_capacity,updated_at_ms=excluded.updated_at_ms;");
 int i=1; sqlite3_bind_int(st.get(),i++,static_cast<int>(kind)); sqlite3_bind_int(st.get(),i++,x.state_version); bind_i64(st.get(),i++,x.attempts); bind_i64(st.get(),i++,x.successes); bind_i64(st.get(),i++,x.failures); bind_i64(st.get(),i++,x.timeouts); bind_i64(st.get(),i++,x.handshake_failures); bind_i64(st.get(),i++,x.unsupported); bind_i64(st.get(),i++,x.consecutive_failures); bind_i64(st.get(),i++,x.consecutive_successes); bind_i64(st.get(),i++,x.latency_avg_ms); bind_i64(st.get(),i++,static_cast<std::int64_t>(x.learned_capacity)); bind_i64(st.get(),i++,x.updated_at_ms); check_rc(db_,sqlite3_step(st.get()),DatabaseErrorCode::TransactionFailed,"save scheduler state failed");
}
std::vector<SchedulerStateRecord> PeerDatabase::load_scheduler_state() const {
 std::lock_guard lock(mutex_); require_open(); std::vector<SchedulerStateRecord> out; Statement st(db_,"SELECT transport,state_version,attempts,successes,failures,timeouts,handshake_failures,unsupported,consecutive_failures,consecutive_successes,latency_avg_ms,learned_capacity,updated_at_ms FROM scheduler_state;");
 while(sqlite3_step(st.get())==SQLITE_ROW){ SchedulerStateRecord r; r.transport=static_cast<transport::TransportKind>(sqlite3_column_int(st.get(),0)); r.state.state_version=static_cast<std::uint32_t>(sqlite3_column_int(st.get(),1)); r.state.attempts=sqlite3_column_int64(st.get(),2); r.state.successes=sqlite3_column_int64(st.get(),3); r.state.failures=sqlite3_column_int64(st.get(),4); r.state.timeouts=sqlite3_column_int64(st.get(),5); r.state.handshake_failures=sqlite3_column_int64(st.get(),6); r.state.unsupported=sqlite3_column_int64(st.get(),7); r.state.consecutive_failures=sqlite3_column_int64(st.get(),8); r.state.consecutive_successes=sqlite3_column_int64(st.get(),9); r.state.latency_avg_ms=sqlite3_column_int64(st.get(),10); r.state.learned_capacity=static_cast<std::size_t>(sqlite3_column_int64(st.get(),11)); r.state.updated_at_ms=sqlite3_column_int64(st.get(),12); out.push_back(r); } return out;
}
bool PeerDatabase::scheduler_state_fresh(std::int64_t updated_at_ms,std::int64_t max_age_ms) const noexcept { if(updated_at_ms<=0||max_age_ms<0)return false; auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); return updated_at_ms<=now && now-updated_at_ms<=max_age_ms; }

} // namespace fastseed::db
