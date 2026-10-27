#include "fastseed/db/peer_database.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sqlite3.h>

using namespace fastseed;

static health::PeerHealth make_peer(crawler::CandidateNetwork n, const char* host, transport::TransportKind tk) {
    health::PeerHealth p;
    p.host = host; p.port = 8226; p.network = n; p.transport = tk; p.source = crawler::CandidateSource::AddrV2;
    p.state = health::PeerState::Quarantined; p.address_bytes.assign(32, n == crawler::CandidateNetwork::TorV3 ? 0x44 : 0x55);
    const auto now = std::chrono::system_clock::now();
    p.first_seen = now; p.last_seen = now; p.last_success = now; p.last_failure = now;
    p.next_probe = now + std::chrono::seconds(123); p.quarantine_until = now + std::chrono::seconds(456);
    p.successful_probes = 7; p.failed_probes = 3; p.consecutive_failures = 2;
    p.transport_attempts = 10; p.transport_successes = 7; p.transport_failures = 3;
    p.protocol_version = 70016; p.services = 1; p.user_agent = "/BSTYFastSeed:0.10.8/";
    p.last_error = "transport failure"; p.transport_last_error = "adapter timeout";
    p.latency_history = {std::chrono::milliseconds(10), std::chrono::milliseconds(20)};
    p.latency_last = std::chrono::milliseconds(20); p.latency_avg = std::chrono::milliseconds(15); p.latency_p95 = std::chrono::milliseconds(20);
    return p;
}

int main() {
    const auto path = std::filesystem::temp_directory_path() / "bsty_fastseed_phase108.sqlite3";
    std::error_code ec; std::filesystem::remove(path, ec); std::filesystem::remove(path.string()+"-wal", ec); std::filesystem::remove(path.string()+"-shm", ec);
    db::PeerDatabase db({path.string(), 250000, std::chrono::hours(24*30), std::chrono::hours(24*14), 2500});
    db.open();
    assert(db.migrate() == 0); // open() completes all current schema migrations.
    auto tor = make_peer(crawler::CandidateNetwork::TorV3, "onion:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", transport::TransportKind::TorSocks5);
    auto i2p = make_peer(crawler::CandidateNetwork::I2P, "i2p:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", transport::TransportKind::I2pSam);
    db.save_batch({tor, i2p});
    db.close();

    sqlite3* raw = nullptr; assert(sqlite3_open(path.c_str(), &raw) == SQLITE_OK);
    sqlite3_stmt* st = nullptr; assert(sqlite3_prepare_v2(raw, "SELECT MAX(version) FROM schema_version", -1, &st, nullptr) == SQLITE_OK);
    assert(sqlite3_step(st) == SQLITE_ROW && sqlite3_column_int(st,0) == 4); sqlite3_finalize(st); sqlite3_close(raw);

    db.open();
    assert(db.migrate() == 0); // open() completes all current schema migrations.
    auto loaded = db.load_health(); assert(loaded.size() == 2);
    bool saw_tor=false, saw_i2p=false;
    for (const auto& p : loaded) {
        if (p.network == crawler::CandidateNetwork::TorV3) {
            saw_tor=true; assert(p.transport == transport::TransportKind::TorSocks5); assert(p.transport_attempts==10 && p.transport_successes==7 && p.transport_failures==3); assert(p.transport_last_error=="adapter timeout"); assert(p.next_probe.time_since_epoch().count()!=0); assert(p.quarantine_until.time_since_epoch().count()!=0);
        } else if (p.network == crawler::CandidateNetwork::I2P) {
            saw_i2p=true; assert(p.transport == transport::TransportKind::I2pSam); assert(p.address_bytes.size()==32);
        }
    }
    assert(saw_tor && saw_i2p);
    assert(db.integrity_check());
    db.close();
    std::filesystem::remove(path, ec); std::filesystem::remove(path.string()+"-wal", ec); std::filesystem::remove(path.string()+"-shm", ec);

    // Exercise automatic v2 -> current v4 migration on an existing-style database.
    const auto legacy = std::filesystem::temp_directory_path() / "bsty_fastseed_phase108_v2.sqlite3";
    std::filesystem::remove(legacy, ec);
    sqlite3* olddb = nullptr; assert(sqlite3_open(legacy.c_str(), &olddb) == SQLITE_OK);
    const char* schema = R"SQL(
CREATE TABLE schema_version(version INTEGER NOT NULL);
INSERT INTO schema_version VALUES(2);
CREATE TABLE peers(
 id INTEGER PRIMARY KEY, address TEXT NOT NULL, address_bytes TEXT NOT NULL DEFAULT '',
 port INTEGER NOT NULL CHECK(port BETWEEN 1 AND 65535), network INTEGER NOT NULL CHECK(network IN (1,2,4,5)),
 protocol_version INTEGER NOT NULL DEFAULT 0, services INTEGER NOT NULL DEFAULT 0, user_agent TEXT NOT NULL DEFAULT '',
 state TEXT NOT NULL, source TEXT NOT NULL, first_seen INTEGER NOT NULL, last_seen INTEGER NOT NULL,
 last_success INTEGER NOT NULL DEFAULT 0, last_failure INTEGER NOT NULL DEFAULT 0, successful_probes INTEGER NOT NULL DEFAULT 0,
 failed_probes INTEGER NOT NULL DEFAULT 0, consecutive_successes INTEGER NOT NULL DEFAULT 0, consecutive_failures INTEGER NOT NULL DEFAULT 0,
 observations INTEGER NOT NULL DEFAULT 0, latency_last_ms INTEGER NOT NULL DEFAULT 0, latency_avg_ms INTEGER NOT NULL DEFAULT 0,
 latency_p95_ms INTEGER NOT NULL DEFAULT 0, latency_history TEXT NOT NULL DEFAULT '', next_probe INTEGER NOT NULL DEFAULT 0,
 quarantine_until INTEGER NOT NULL DEFAULT 0, ipv4_prefix TEXT NOT NULL DEFAULT '', ipv6_prefix TEXT NOT NULL DEFAULT '',
 asn INTEGER NOT NULL DEFAULT 0, country_code TEXT NOT NULL DEFAULT '', last_error TEXT NOT NULL DEFAULT '', UNIQUE(address,port));
)SQL";
    char* errmsg=nullptr; assert(sqlite3_exec(olddb, schema, nullptr, nullptr, &errmsg) == SQLITE_OK); if(errmsg) sqlite3_free(errmsg); sqlite3_close(olddb);
    db::PeerDatabase migrated({legacy.string(), 250000, std::chrono::hours(24*30), std::chrono::hours(24*14), 2500});
    migrated.open();
    assert(migrated.migrate() == 0); // Legacy databases also reach current schema.
    auto migrated_peer = make_peer(crawler::CandidateNetwork::TorV3, "onion:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", transport::TransportKind::TorSocks5);
    migrated.save(migrated_peer);
    auto migrated_loaded = migrated.load_health(); assert(migrated_loaded.size() == 1);
    assert(migrated_loaded.front().transport == transport::TransportKind::TorSocks5);
    assert(migrated_loaded.front().transport_attempts == migrated_peer.transport_attempts);
    assert(migrated.integrity_check()); migrated.close();
    std::filesystem::remove(legacy, ec); std::filesystem::remove(legacy.string()+"-wal", ec); std::filesystem::remove(legacy.string()+"-shm", ec);

    std::cout << "phase10.8 transport persistence tests passed\n";
}
