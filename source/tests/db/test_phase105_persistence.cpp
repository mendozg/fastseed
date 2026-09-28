#include "fastseed/db/peer_database.hpp"
#include "fastseed/health/peer_health.hpp"
#include "fastseed/protocol/addrv2.hpp"
#include "fastseed/crawler/candidate.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>

using namespace fastseed;

static health::PeerHealth make_addr_peer(const protocol::AddrV2Address& a, health::PeerState state) {
    auto c = crawler::make_addrv2_candidate(a, crawler::CandidateSource::AddrV2);
    health::PeerHealth p;
    p.host = c.host;
    p.port = c.port;
    p.source = c.source;
    p.network = c.network;
    p.address_bytes = c.address_bytes;
    p.state = state;
    auto now = std::chrono::system_clock::now();
    p.first_seen = now;
    p.last_seen = now;
    p.last_success = now;
    p.observations = 3;
    p.successful_probes = 3;
    p.consecutive_successes = 3;
    p.protocol_version = 70016;
    p.user_agent = "/BSTYFastSeed:0.10.5/";
    p.latency_history = {std::chrono::milliseconds(10), std::chrono::milliseconds(20), std::chrono::milliseconds(30)};
    p.latency_last = p.latency_history.back();
    p.latency_avg = std::chrono::milliseconds(20);
    p.latency_p95 = std::chrono::milliseconds(30);
    return p;
}

int main() {
    const auto path = std::filesystem::temp_directory_path() / "bsty_fastseed_phase105.sqlite3";
    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(path.string()+"-wal", ec);
    std::filesystem::remove(path.string()+"-shm", ec);

    db::PeerDatabase db({path.string(), 250000, std::chrono::hours(24*30), std::chrono::hours(24*14), 2500});
    db.open();

    protocol::AddrV2Address tor{};
    tor.timestamp = 1;
    tor.services = 1;
    tor.network_id = 0x04;
    tor.address.resize(32);
    for (std::size_t i=0; i<tor.address.size(); ++i) tor.address[i] = static_cast<std::uint8_t>(i);
    tor.port = 8226;

    protocol::AddrV2Address i2p = tor;
    i2p.network_id = 0x05;
    i2p.address[0] = 0xaa;

    auto tor_health = make_addr_peer(tor, health::PeerState::Warm);
    auto i2p_health = make_addr_peer(i2p, health::PeerState::Cold);
    db.save_batch({tor_health, i2p_health});

    auto loaded = db.load_health();
    assert(loaded.size() == 2);
    bool saw_tor = false, saw_i2p = false;
    for (const auto& p : loaded) {
        if (p.network == crawler::CandidateNetwork::TorV3) {
            saw_tor = true;
            assert(p.address_bytes == tor.address);
            assert(p.host.rfind("onion:", 0) == 0);
        }
        if (p.network == crawler::CandidateNetwork::I2P) {
            saw_i2p = true;
            assert(p.address_bytes == i2p.address);
            assert(p.host.rfind("i2p:", 0) == 0);
        }
    }
    assert(saw_tor && saw_i2p);

    auto candidates = db.load_candidates();
    assert(candidates.size() == 2);
    for (const auto& c : candidates) {
        assert(c.source == crawler::CandidateSource::PersistentDatabase);
        assert(c.state == crawler::CandidateState::AwaitingTransport);
        if (c.network == crawler::CandidateNetwork::TorV3) assert(c.address_bytes == tor.address);
        if (c.network == crawler::CandidateNetwork::I2P) assert(c.address_bytes == i2p.address);
    }

    health::PeerHealthCache cache;
    cache.restore(loaded);
    assert(cache.contains(tor_health.key()));
    assert(cache.contains(i2p_health.key()));
    auto dns = cache.dns_snapshot(32, true);
    assert(dns.empty()); // overlay-only networks must never enter A/AAAA DNS output
    auto stats = cache.stats();
    assert(stats.total == 2);

    assert(db.integrity_check());
    db.close();
    std::filesystem::remove(path, ec);
    std::filesystem::remove(path.string()+"-wal", ec);
    std::filesystem::remove(path.string()+"-shm", ec);
    std::cout << "phase10.5 persistence tests passed\n";
}
