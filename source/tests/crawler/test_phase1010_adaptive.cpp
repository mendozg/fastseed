#include "fastseed/crawler/scheduler.hpp"
#include <cassert>
#include <chrono>
#include <vector>
using namespace fastseed;
int main() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    health::PeerHealth good;
    good.host="1.2.3.4"; good.port=8226; good.network=crawler::CandidateNetwork::IPv4;
    good.state=health::PeerState::Hot; good.last_success=now-hours(2);
    good.successful_probes=20; good.failed_probes=0; good.latency_avg=milliseconds(100);

    health::PeerHealth bad;
    bad.host="5.6.7.8"; bad.port=8226; bad.network=crawler::CandidateNetwork::IPv4;
    bad.state=health::PeerState::Warm; bad.last_success=now-minutes(10);
    bad.successful_probes=2; bad.failed_probes=4; bad.latency_avg=milliseconds(2000);

    health::PeerHealth tor;
    tor.host="onion:" + std::string(64, 'a'); tor.port=8226;
    tor.network=crawler::CandidateNetwork::TorV3; tor.state=health::PeerState::Hot;
    tor.last_success=now-hours(2); tor.successful_probes=10; tor.latency_avg=milliseconds(100);

    crawler::AdaptiveSchedulerConfig cfg;
    cfg.max_batch=10;
    cfg.direct_tcp={2, 2}; cfg.tor_socks5={1, 1}; cfg.i2p_sam={1, 1};
    cfg.hot_refresh=minutes(30); cfg.warm_refresh=minutes(10);
    cfg.minimum_refresh=seconds(30); cfg.maximum_refresh=hours(2);
    crawler::AdaptiveProbeScheduler scheduler(cfg);

    assert(scheduler.recommended_refresh(good) == hours(1));
    assert(scheduler.recommended_refresh(bad) == minutes(5));

    auto due = scheduler.select_due({good, bad, tor}, {}, now);
    assert(due.size() == 3);
    assert(due[0].candidate.network == crawler::CandidateNetwork::IPv4);
    assert(due[1].candidate.network == crawler::CandidateNetwork::IPv4);
    assert(due[2].candidate.network == crawler::CandidateNetwork::TorV3);

    auto capped = scheduler.select_due({good, bad, tor}, {2, 0, 0}, now);
    assert(capped.size() == 1);
    assert(capped[0].candidate.network == crawler::CandidateNetwork::TorV3);

    auto tor_capped = scheduler.select_due({good, bad, tor}, {0, 1, 0}, now);
    assert(tor_capped.size() == 2);
    assert(tor_capped[1].candidate.network == crawler::CandidateNetwork::IPv4);
    assert(tor_capped[0].candidate.network == crawler::CandidateNetwork::IPv4);
    return 0;
}
