#include "fastseed/crawler/scheduler.hpp"
#include "fastseed/crawler/adaptive_feedback.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

static fastseed::health::PeerHealth peer(const std::string& host, fastseed::crawler::CandidateNetwork n) {
    fastseed::health::PeerHealth p; p.host=host; p.port=8226; p.network=n; p.state=fastseed::health::PeerState::Warm; return p;
}

int main() {
    using namespace fastseed; using namespace std::chrono_literals;
    crawler::AdaptiveSchedulerConfig sc;
    sc.max_batch=64; sc.exploration_fraction=.20; sc.minimum_exploration=1; sc.maximum_exploration=12;
    sc.direct_tcp={40,40}; sc.tor_socks5={12,12}; sc.i2p_sam={12,12}; sc.exploration_rotation=60s;
    crawler::FeedbackConfig fc; fc.global_max_inflight=64; fc.direct_tcp={40,40}; fc.tor_socks5={12,12}; fc.i2p_sam={12,12};
    fc.learning_half_life=1h;
    crawler::AdaptiveCapacityController controller(fc); crawler::AdaptiveProbeScheduler scheduler(sc);
    std::vector<health::PeerHealth> peers;
    for(int i=0;i<30;++i) peers.push_back(peer("10.10.0."+std::to_string(i+1), crawler::CandidateNetwork::IPv4));
    for(int i=0;i<9;++i) peers.push_back(peer("onion:p"+std::to_string(i), crawler::CandidateNetwork::TorV3));
    for(int i=0;i<9;++i) peers.push_back(peer("i2p:p"+std::to_string(i), crawler::CandidateNetwork::I2P));
    auto now=std::chrono::system_clock::now();
    std::uint64_t previous_cycles=0, previous_used=0;
    // 7 simulated days at 5-minute scheduling intervals.
    for(int cycle=0; cycle<7*24*12; ++cycle) {
        // Changing network conditions: direct degrades for a day, Tor recovers,
        // then I2P degrades. The controller must remain bounded throughout.
        if(cycle%12==0) {
            const int hour=(cycle/12)%24; const int day=cycle/(24*12);
            if(day==1 && hour<24) controller.record(transport::TransportKind::DirectTcp, crawler::ProbeOutcome::Timeout, 1500ms);
            if(day>=2 && day<=3) controller.record(transport::TransportKind::TorSocks5, crawler::ProbeOutcome::Success, 180ms);
            if(day==4) controller.record(transport::TransportKind::I2pSam, crawler::ProbeOutcome::Timeout, 1500ms);
            if(day>=5) controller.record(transport::TransportKind::DirectTcp, crawler::ProbeOutcome::Success, 120ms);
        }
        if(cycle%6==0) {
            controller.decay(transport::TransportKind::DirectTcp, now);
            controller.decay(transport::TransportKind::TorSocks5, now);
            controller.decay(transport::TransportKind::I2pSam, now);
        }
        crawler::ActiveProbeCounts active{static_cast<std::size_t>(cycle%5), static_cast<std::size_t>(cycle%3), static_cast<std::size_t>(cycle%2)};
        auto out=scheduler.select_due(peers, controller, active, now);
        const std::size_t active_total=active.direct_tcp+active.tor_socks5+active.i2p_sam;
        assert(active_total <= fc.global_max_inflight);
        assert(out.size() <= fc.global_max_inflight-active_total);
        for(const auto& s:out) assert(s.candidate.port==8226);
        assert(controller.capacity(transport::TransportKind::DirectTcp) <= 40);
        assert(controller.capacity(transport::TransportKind::TorSocks5) <= 12);
        assert(controller.capacity(transport::TransportKind::I2pSam) <= 12);
        assert(controller.confidence(transport::TransportKind::DirectTcp, now)>=0.0 && controller.confidence(transport::TransportKind::DirectTcp, now)<=1.0);
        assert(controller.confidence(transport::TransportKind::TorSocks5, now)>=0.0 && controller.confidence(transport::TransportKind::TorSocks5, now)<=1.0);
        assert(controller.confidence(transport::TransportKind::I2pSam, now)>=0.0 && controller.confidence(transport::TransportKind::I2pSam, now)<=1.0);
        const auto t=scheduler.telemetry();
        assert(t.scheduling_cycles>=previous_cycles && t.exploration_used>=previous_used);
        previous_cycles=t.scheduling_cycles; previous_used=t.exploration_used;
        now += 5min;
    }
    const auto t=scheduler.telemetry();
    assert(t.scheduling_cycles==7*24*12);
    assert(t.exploration_used <= t.exploration_budget);
    std::cout << "phase1024 long-run stress tests: PASS (7 simulated days, " << t.scheduling_cycles << " cycles)\n";
}
