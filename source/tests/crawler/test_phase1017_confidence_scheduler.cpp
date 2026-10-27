#include "fastseed/crawler/scheduler.hpp"
#include "fastseed/crawler/adaptive_feedback.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

int main() {
    using namespace fastseed;
    using namespace std::chrono_literals;
    crawler::AdaptiveSchedulerConfig scfg;
    scfg.max_batch = 8;
    crawler::AdaptiveProbeScheduler scheduler(scfg);
    crawler::FeedbackConfig fcfg;
    fcfg.global_max_inflight = 8;
    fcfg.direct_tcp = {1, 2, 4};
    fcfg.tor_socks5 = {1, 2, 4};
    fcfg.i2p_sam = {1, 2, 4};
    crawler::AdaptiveCapacityController controller(fcfg);

    health::PeerHealth direct;
    direct.host = "1.2.3.4"; direct.port = 8226;
    direct.network = crawler::CandidateNetwork::IPv4;
    direct.state = health::PeerState::Warm;

    health::PeerHealth tor;
    tor.host = "onion:test"; tor.port = 8226;
    tor.network = crawler::CandidateNetwork::TorV3;
    tor.state = health::PeerState::Warm;

    std::vector<health::PeerHealth> peers{direct, tor};
    auto out = scheduler.select_due(peers, controller, {}, std::chrono::system_clock::now());
    assert(out.size() == 2);
    assert(out.front().priority > 0);

    for (int i = 0; i < 32; ++i)
        controller.record(transport::TransportKind::DirectTcp, crawler::ProbeOutcome::Success, 20ms);
    auto out2 = scheduler.select_due(peers, controller, {}, std::chrono::system_clock::now());
    assert(out2.size() == 2);
    assert(out2.front().candidate.network == crawler::CandidateNetwork::TorV3);

    crawler::ActiveProbeCounts active;
    active.direct_tcp = 4;
    auto capped = scheduler.select_due(peers, controller, active, std::chrono::system_clock::now());
    for (const auto& item : capped) assert(item.candidate.network != crawler::CandidateNetwork::IPv4);

    // Phase 10.18: exploration is budgeted and cannot consume the whole batch.
    crawler::AdaptiveSchedulerConfig budget_cfg;
    budget_cfg.max_batch = 10;
    budget_cfg.exploration_fraction = 0.20;
    budget_cfg.minimum_exploration = 1;
    budget_cfg.maximum_exploration = 2;
    budget_cfg.low_confidence_threshold = 0.50;
    crawler::AdaptiveProbeScheduler budget_scheduler(budget_cfg);
    std::vector<health::PeerHealth> many;
    for (int i = 0; i < 10; ++i) {
        health::PeerHealth p; p.host = "10.0.0." + std::to_string(i + 1); p.port = 8226;
        p.network = crawler::CandidateNetwork::IPv4; p.state = health::PeerState::Warm;
        many.push_back(p);
    }
    auto budgeted = budget_scheduler.select_due(many, controller, {}, std::chrono::system_clock::now());
    // Direct capacity is four, even when the batch has room for ten.
    assert(budgeted.size() == 4);
    assert(budgeted.size() <= controller.capacity(transport::TransportKind::DirectTcp));
    // With no evidence, all are exploration candidates, but the batch still obeys max_batch.
    assert(budgeted.size() <= budget_cfg.max_batch);

    std::cout << "phase1017 confidence-aware scheduler tests: PASS\n";
    std::cout << "phase1018 exploration budget tests: PASS\n";
}
