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
        p.host = "10.0.0." + std::to_string(i + 1);
        p.port = 8226; p.network = crawler::CandidateNetwork::IPv4;
        p.state = health::PeerState::Warm; v.push_back(p);
        p.host = "onion:peer" + std::to_string(i + 1);
        p.network = crawler::CandidateNetwork::TorV3; v.push_back(p);
        p.host = "i2p:peer" + std::to_string(i + 1);
        p.network = crawler::CandidateNetwork::I2P; v.push_back(p);
    }
    return v;
}

int main() {
    using namespace fastseed; using namespace std::chrono_literals;
    crawler::AdaptiveSchedulerConfig cfg;
    cfg.max_batch = 2; cfg.exploration_fraction = 1.0;
    cfg.minimum_exploration = 2; cfg.maximum_exploration = 2;
    cfg.low_confidence_threshold = 0.50; cfg.exploration_rotation = 60s;
    cfg.direct_tcp = {10,10}; cfg.tor_socks5 = {10,10}; cfg.i2p_sam = {10,10};
    crawler::AdaptiveProbeScheduler scheduler(cfg);
    crawler::FeedbackConfig fc; fc.global_max_inflight = 2;
    fc.direct_tcp={1,10}; fc.tor_socks5={1,10}; fc.i2p_sam={1,10};
    crawler::AdaptiveCapacityController controller(fc);
    auto ps = peers();
    const auto base = std::chrono::system_clock::time_point{std::chrono::seconds{1800000000}};
    bool direct=false, tor=false, i2p=false;
    for (int bucket=0; bucket<3; ++bucket) {
        auto out = scheduler.select_due(ps, controller, {}, base + bucket * 60s);
        assert(out.size() == 2);
        bool d=false,t=false,i=false;
        for (auto const& x: out) {
            d |= x.candidate.network == crawler::CandidateNetwork::IPv4;
            t |= x.candidate.network == crawler::CandidateNetwork::TorV3;
            i |= x.candidate.network == crawler::CandidateNetwork::I2P;
        }
        direct |= d; tor |= t; i2p |= i;
    }
    assert(direct && tor && i2p); // no transport starves across rotation windows
    std::cout << "phase1019 exploration fairness tests: PASS\n";
}
