#include "fastseed/crawler/scheduler.hpp"
#include "fastseed/crawler/adaptive_feedback.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

int main() {
    using namespace fastseed;
    using namespace std::chrono_literals;

    crawler::AdaptiveSchedulerConfig cfg;
    cfg.max_batch = 10;
    cfg.exploration_fraction = 0.20;
    cfg.minimum_exploration = 1;
    cfg.maximum_exploration = 2;
    cfg.low_confidence_threshold = 0.50;
    cfg.direct_tcp = {10, 10};
    cfg.tor_socks5 = {10, 10};
    cfg.i2p_sam = {10, 10};
    crawler::AdaptiveProbeScheduler scheduler(cfg);

    crawler::FeedbackConfig fc;
    fc.global_max_inflight = 10;
    fc.direct_tcp = {10, 10};
    fc.tor_socks5 = {10, 10};
    fc.i2p_sam = {10, 10};
    crawler::AdaptiveCapacityController controller(fc);

    std::vector<health::PeerHealth> peers;
    for (int i = 0; i < 10; ++i) {
        health::PeerHealth p;
        p.host = "10.0.0." + std::to_string(i + 1);
        p.port = 8226;
        p.network = crawler::CandidateNetwork::IPv4;
        p.state = health::PeerState::Warm;
        peers.push_back(p);
    }

    auto out = scheduler.select_due(peers, controller, {}, std::chrono::system_clock::now());
    assert(out.size() == 10); // idle capacity may be used for exploration when nothing is established.

    // Establish Direct TCP evidence while Tor remains low-confidence. With a 20%
    // exploration budget, the established transport should fill the remainder first.
    for (int i = 0; i < 32; ++i)
        controller.record(transport::TransportKind::DirectTcp, crawler::ProbeOutcome::Success, 20ms);
    std::vector<health::PeerHealth> mixed;
    for (int i = 0; i < 8; ++i) {
        auto p = peers[i]; p.host = "10.0.0." + std::to_string(i + 1); mixed.push_back(p);
    }
    for (int i = 0; i < 8; ++i) {
        health::PeerHealth p; p.host = "onion:peer" + std::to_string(i + 1); p.port = 8226;
        p.network = crawler::CandidateNetwork::TorV3; p.state = health::PeerState::Warm;
        mixed.push_back(p);
    }
    auto mixed_out = scheduler.select_due(mixed, controller, {}, std::chrono::system_clock::now());
    assert(mixed_out.size() == 10);
    std::size_t tor_count = 0;
    for (const auto& item : mixed_out)
        if (item.candidate.network == crawler::CandidateNetwork::TorV3) ++tor_count;
    assert(tor_count == 2); // exactly the configured 20% exploration budget.

    crawler::ActiveProbeCounts active;
    active.direct_tcp = 5;
    auto capped = scheduler.select_due(peers, controller, active, std::chrono::system_clock::now());
    assert(capped.size() <= 5);

    std::cout << "phase1018 exploration budget tests: PASS\n";
}
