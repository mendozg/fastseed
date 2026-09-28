#include "fastseed/health/peer_health.hpp"
#include <chrono>
#include <iostream>

using namespace fastseed::health;
using namespace fastseed::crawler;
using fastseed::protocol::ProbeResult;
using fastseed::protocol::ProbeStatus;

static int require(bool v, const char* what) {
    if (!v) { std::cerr << "FAIL: " << what << "\n"; return 1; }
    return 0;
}

static PeerCandidate candidate(const char* host) {
    PeerCandidate c;
    c.host = host; c.port = 8226; c.source = CandidateSource::Addr;
    return c;
}

int main() {
    HealthConfig cfg;
    cfg.minimum_observations_for_hot = 3;
    cfg.quarantine_after_failures = 3;
    cfg.latency_history_size = 4;
    cfg.hot_ipv4 = 2;
    cfg.hot_ipv6 = 1;
    cfg.warm_max = 4;
    cfg.cold_max = 4;

    PeerHealthCache cache(cfg);
    auto c = candidate("127.0.0.1");
    ProbeResult ok;
    ok.status = ProbeStatus::Success;
    ok.protocol_version = 70016;
    ok.services = 0;
    ok.user_agent = "/BSTYFastSeed:0.1/";
    ok.connect_time = std::chrono::milliseconds(20);
    ok.handshake_time = std::chrono::milliseconds(40);

    cache.observe_success(c, ok);
    auto* p = cache.find(c.key());
    if (require(p != nullptr, "peer created")) return 1;
    if (require(p->state == PeerState::Warm, "first success warm")) return 1;
    if (require(p->successful_probes == 1, "first success counted")) return 1;
    if (require(p->latency_last.count() == 40, "latency recorded")) return 1;

    cache.observe_success(c, ok);
    cache.observe_success(c, ok);
    p = cache.find(c.key());
    if (require(p->state == PeerState::Hot, "third success hot")) return 1;
    if (require(p->consecutive_successes == 3, "consecutive successes")) return 1;
    if (require(p->latency_avg.count() == 40, "average latency")) return 1;
    if (require(p->latency_p95.count() == 40, "p95 latency")) return 1;

    auto dns = cache.dns_snapshot(10);
    if (require(dns.size() == 1 && dns.front().key() == c.key(), "dns snapshot hot peer")) return 1;

    ProbeResult bad;
    bad.status = ProbeStatus::Timeout;
    bad.error = "timeout";
    cache.observe_failure(c, bad);
    p = cache.find(c.key());
    if (require(p->state == PeerState::Cold, "hot peer degrades to cold")) return 1;
    if (require(p->consecutive_failures == 1, "failure streak")) return 1;

    cache.observe_failure(c, bad);
    cache.observe_failure(c, bad);
    p = cache.find(c.key());
    if (require(p->state == PeerState::Quarantined, "failure quarantine")) return 1;
    if (require(cache.dns_snapshot(10).empty(), "quarantined peer excluded")) return 1;

    auto s = cache.stats();
    if (require(s.total == 1 && s.observations == 6 && s.successful_probes == 3 && s.failed_probes == 3, "stats")) return 1;

    cache.clear();
    if (require(cache.snapshot().empty(), "clear")) return 1;
    std::cout << "phase3 health cache tests passed\n";
}
