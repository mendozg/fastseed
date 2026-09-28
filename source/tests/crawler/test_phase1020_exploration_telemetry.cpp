#include "fastseed/crawler/scheduler.hpp"
#include "fastseed/crawler/adaptive_feedback.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

static std::vector<fastseed::health::PeerHealth> peers() {
    using namespace fastseed;
    std::vector<health::PeerHealth> v;
    for (int i = 0; i < 3; ++i) {
        health::PeerHealth p;
        p.host = "10.20.0." + std::to_string(i + 1); p.port = 8226;
        p.network = crawler::CandidateNetwork::IPv4; p.state = health::PeerState::Warm; v.push_back(p);
        p.host = "onion:telemetry" + std::to_string(i + 1); p.network = crawler::CandidateNetwork::TorV3; v.push_back(p);
        p.host = "i2p:telemetry" + std::to_string(i + 1); p.network = crawler::CandidateNetwork::I2P; v.push_back(p);
    }
    return v;
}

int main() {
    using namespace fastseed; using namespace std::chrono_literals;
    crawler::AdaptiveSchedulerConfig cfg;
    cfg.max_batch = 3; cfg.exploration_fraction = 1.0;
    cfg.minimum_exploration = 3; cfg.maximum_exploration = 3;
    cfg.low_confidence_threshold = 0.50; cfg.exploration_rotation = 60s;
    cfg.direct_tcp = {10,10}; cfg.tor_socks5 = {10,10}; cfg.i2p_sam = {10,10};
    crawler::AdaptiveProbeScheduler scheduler(cfg);
    crawler::FeedbackConfig fc; fc.global_max_inflight = 3;
    fc.direct_tcp={1,10,10}; fc.tor_socks5={1,10,10}; fc.i2p_sam={1,10,10};
    crawler::AdaptiveCapacityController controller(fc);
    auto ps = peers();
    const auto base = std::chrono::system_clock::time_point{std::chrono::seconds{1800000000}};
    for (int bucket = 0; bucket < 3; ++bucket) {
        auto out = scheduler.select_due(ps, controller, {}, base + bucket * 60s);
        assert(out.size() == 3);
    }
    auto t = scheduler.telemetry();
    assert(t.scheduling_cycles == 3);
    assert(t.exploration_budget == 9);
    assert(t.exploration_used == 9);
    for (const auto& x : t.transport) {
        assert(x.budget_allocated >= 1);
        assert(x.opportunities >= 1);
        assert(x.probes_selected >= 1);
    }
    std::cout << "phase1020 exploration telemetry tests: PASS\n";
}
