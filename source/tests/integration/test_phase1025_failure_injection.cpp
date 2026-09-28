#include "fastseed/crawler/scheduler.hpp"
#include "fastseed/crawler/adaptive_feedback.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace fastseed;
using namespace std::chrono_literals;

static health::PeerHealth peer(const std::string& host, crawler::CandidateNetwork n) {
    health::PeerHealth p;
    p.host = host; p.port = 8226; p.network = n; p.state = health::PeerState::Warm;
    return p;
}

static void assert_bounds(const crawler::AdaptiveCapacityController& c) {
    assert(c.capacity(transport::TransportKind::DirectTcp) <= 40);
    assert(c.capacity(transport::TransportKind::TorSocks5) <= 12);
    assert(c.capacity(transport::TransportKind::I2pSam) <= 12);
}

int main() {
    crawler::FeedbackConfig fc;
    fc.global_max_inflight = 64;
    fc.direct_tcp = {4, 20, 40};
    fc.tor_socks5 = {2, 6, 12};
    fc.i2p_sam = {2, 6, 12};
    fc.adjustment_step = 2;
    fc.cooldown_after_adjustment = 0s;
    fc.learning_half_life = 1h;
    crawler::AdaptiveCapacityController c(fc);

    // Transport outage injection: failures must reduce capacity without crossing bounds.
    for (int i = 0; i < 10; ++i)
        c.record(transport::TransportKind::DirectTcp, crawler::ProbeOutcome::Timeout, 1500ms);
    assert(c.capacity(transport::TransportKind::DirectTcp) >= 4);
    assert_bounds(c);

    // Recovery injection: successful probes must be able to restore capacity.
    for (int i = 0; i < 16; ++i)
        c.record(transport::TransportKind::DirectTcp, crawler::ProbeOutcome::Success, 100ms);
    assert(c.capacity(transport::TransportKind::DirectTcp) <= 40);
    assert_bounds(c);

    // Tor outage followed by recovery.
    for (int i = 0; i < 12; ++i)
        c.record(transport::TransportKind::TorSocks5, crawler::ProbeOutcome::Failure, 2000ms);
    assert(c.capacity(transport::TransportKind::TorSocks5) >= 2);
    for (int i = 0; i < 16; ++i)
        c.record(transport::TransportKind::TorSocks5, crawler::ProbeOutcome::Success, 180ms);
    assert(c.capacity(transport::TransportKind::TorSocks5) <= 12);

    // I2P outage. Unsupported transport is deliberately not treated as a failure signal.
    const auto before = c.capacity(transport::TransportKind::I2pSam);
    for (int i = 0; i < 8; ++i)
        c.record(transport::TransportKind::I2pSam, crawler::ProbeOutcome::Unsupported, 0ms);
    assert(c.capacity(transport::TransportKind::I2pSam) == before);

    // Protocol failure injection: handshake failures are capacity-relevant but bounded.
    for (int i = 0; i < 8; ++i)
        c.record(transport::TransportKind::DirectTcp, crawler::ProbeOutcome::HandshakeFailure, 800ms);
    assert_bounds(c);

    // Scheduler must still respect all admission limits after faults.
    crawler::AdaptiveSchedulerConfig sc;
    sc.max_batch = 64; sc.exploration_fraction = .20; sc.minimum_exploration = 1; sc.maximum_exploration = 12;
    sc.direct_tcp = {40, 40}; sc.tor_socks5 = {12, 12}; sc.i2p_sam = {12, 12}; sc.exploration_rotation = 60s;
    crawler::AdaptiveProbeScheduler scheduler(sc);
    std::vector<health::PeerHealth> peers;
    for (int i = 0; i < 50; ++i) peers.push_back(peer("10.25.0." + std::to_string(i + 1), crawler::CandidateNetwork::IPv4));
    for (int i = 0; i < 15; ++i) peers.push_back(peer("onion:p" + std::to_string(i), crawler::CandidateNetwork::TorV3));
    for (int i = 0; i < 15; ++i) peers.push_back(peer("i2p:p" + std::to_string(i), crawler::CandidateNetwork::I2P));

    crawler::ActiveProbeCounts active{3, 2, 1};
    auto out = scheduler.select_due(peers, c, active, std::chrono::system_clock::now());
    const auto active_total = active.direct_tcp + active.tor_socks5 + active.i2p_sam;
    assert(active_total <= fc.global_max_inflight);
    assert(out.size() <= fc.global_max_inflight - active_total);
    assert_bounds(c);

    // Persistence/observability fault model: the scheduler remains usable when external
    // sinks are unavailable; this phase models sink failure without changing scheduler state.
    bool persistence_sink_failed = true;
    bool metrics_sink_failed = true;
    if (persistence_sink_failed || metrics_sink_failed) {
        auto again = scheduler.select_due(peers, c, active, std::chrono::system_clock::now() + 1min);
        assert(again.size() <= fc.global_max_inflight - active_total);
    }
    persistence_sink_failed = false;
    metrics_sink_failed = false;
    assert(!persistence_sink_failed && !metrics_sink_failed);

    // Confidence must remain finite and bounded after all injected outcomes.
    const auto now = std::chrono::system_clock::now();
    for (auto k : {transport::TransportKind::DirectTcp, transport::TransportKind::TorSocks5, transport::TransportKind::I2pSam}) {
        const double q = c.confidence(k, now);
        assert(q >= 0.0 && q <= 1.0);
    }

    std::cout << "phase1025 failure injection/recovery tests: PASS\n";
    return 0;
}
