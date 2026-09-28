#include "fastseed/crawler/scheduler.hpp"
#include "fastseed/crawler/adaptive_feedback.hpp"
#include "fastseed/metrics/exploration.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace fastseed;
    using namespace std::chrono_literals;
    crawler::AdaptiveSchedulerConfig sc;
    sc.max_batch = 12; sc.exploration_fraction = .20; sc.minimum_exploration = 1; sc.maximum_exploration = 3;
    sc.direct_tcp = {8,8}; sc.tor_socks5 = {4,4}; sc.i2p_sam = {4,4};
    crawler::FeedbackConfig fc; fc.global_max_inflight = 12; fc.direct_tcp={8,8}; fc.tor_socks5={4,4}; fc.i2p_sam={4,4};
    crawler::AdaptiveCapacityController controller(fc); crawler::AdaptiveProbeScheduler scheduler(sc);
    for (int i=0;i<32;++i) controller.record(transport::TransportKind::DirectTcp, crawler::ProbeOutcome::Success, 20ms);
    std::vector<health::PeerHealth> peers;
    for (int i=0;i<6;++i) { health::PeerHealth p; p.host="10.0.0."+std::to_string(i+1); p.port=8226; p.network=crawler::CandidateNetwork::IPv4; p.state=health::PeerState::Warm; peers.push_back(p); }
    for (int i=0;i<4;++i) { health::PeerHealth p; p.host="onion:p"+std::to_string(i); p.port=8226; p.network=crawler::CandidateNetwork::TorV3; p.state=health::PeerState::Warm; peers.push_back(p); }
    for (int i=0;i<4;++i) { health::PeerHealth p; p.host="i2p:p"+std::to_string(i); p.port=8226; p.network=crawler::CandidateNetwork::I2P; p.state=health::PeerState::Warm; peers.push_back(p); }
    const auto before = scheduler.telemetry();
    auto out = scheduler.select_due(peers, controller, {}, std::chrono::system_clock::now());
    assert(out.size() <= 12);
    const auto snap = scheduler.telemetry();
    assert(snap.scheduling_cycles == before.scheduling_cycles + 1);
    const auto prom = metrics::exploration_prometheus(snap);
    assert(prom.find("fastseed_exploration_cycles_total") != std::string::npos);
    assert(prom.find("transport=\"tor\"") != std::string::npos);
    assert(prom.find("transport=\"i2p\"") != std::string::npos);
    std::cout << "phase1023 integration gate tests: PASS\n";
}
