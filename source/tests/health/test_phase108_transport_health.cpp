#include "fastseed/health/peer_health.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

using namespace fastseed;

static crawler::PeerCandidate candidate(crawler::CandidateNetwork n, const std::string& host) {
    crawler::PeerCandidate c;
    c.host = host; c.port = 8226; c.source = crawler::CandidateSource::AddrV2; c.network = n;
    c.state = c.transport_probe_supported() ? crawler::CandidateState::Queued : crawler::CandidateState::AwaitingTransport;
    if (n == crawler::CandidateNetwork::TorV3) c.address_bytes.assign(32, 0x11);
    if (n == crawler::CandidateNetwork::I2P) c.address_bytes.assign(32, 0x22);
    return c;
}

static protocol::ProbeResult success() {
    protocol::ProbeResult r; r.status = protocol::ProbeStatus::Success; r.protocol_version = 70016;
    r.user_agent = "/BSTYFastSeed:0.10.8/"; r.connect_time = std::chrono::milliseconds(10); r.handshake_time = std::chrono::milliseconds(25); return r;
}

int main() {
    health::HealthConfig cfg;
    cfg.minimum_observations_for_hot = 2;
    cfg.cooldown_initial = std::chrono::seconds(5);
    cfg.cooldown_max = std::chrono::seconds(20);
    cfg.cooldown_multiplier = 2.0;
    health::PeerHealthCache cache(cfg);

    auto tor = candidate(crawler::CandidateNetwork::TorV3, "onion:1111111111111111111111111111111111111111111111111111111111111111");
    auto i2p = candidate(crawler::CandidateNetwork::I2P, "i2p:2222222222222222222222222222222222222222222222222222222222222222");
    auto direct = candidate(crawler::CandidateNetwork::IPv4, "198.51.100.7");

    cache.observe_success(tor, success());
    cache.observe_success(tor, success());
    cache.observe_success(i2p, success());
    auto* tp = cache.find(tor.key());
    assert(tp && tp->transport == transport::TransportKind::TorSocks5);
    assert(tp->transport_attempts == 2 && tp->transport_successes == 2 && tp->transport_failures == 0);
    assert(tp->next_probe.time_since_epoch().count() != 0);
    assert(cache.find(i2p.key())->transport == transport::TransportKind::I2pSam);

    protocol::ProbeResult bad; bad.status = protocol::ProbeStatus::Timeout; bad.error = "SOCKS5 timeout";
    cache.observe_failure(tor, bad);
    tp = cache.find(tor.key());
    assert(tp->transport_attempts == 3 && tp->transport_successes == 2 && tp->transport_failures == 1);
    assert(tp->transport_last_error == "SOCKS5 timeout");
    const auto now = std::chrono::system_clock::now();
    assert(tp->next_probe > now);
    assert(tp->next_probe <= now + std::chrono::seconds(6));

    cache.observe_failure(tor, bad);
    cache.observe_failure(tor, bad);
    tp = cache.find(tor.key());
    assert(tp->state == health::PeerState::Quarantined);
    assert(tp->quarantine_until > now);
    assert(tp->transport_failures == 3);
    assert(cache.dns_snapshot(20).empty());

    cache.observe_success(direct, success());
    auto* dp = cache.find(direct.key());
    assert(dp && dp->transport == transport::TransportKind::DirectTcp);
    assert(dp->transport_attempts == 1);
    std::cout << "phase10.8 transport health tests passed\n";
}
