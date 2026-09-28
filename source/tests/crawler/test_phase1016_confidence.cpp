#include "fastseed/crawler/adaptive_feedback.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>

int main() {
    using namespace fastseed;
    crawler::FeedbackConfig cfg;
    cfg.learning_half_life = std::chrono::hours(1);
    crawler::AdaptiveCapacityController c(cfg);
    const auto k = transport::TransportKind::DirectTcp;
    assert(c.confidence(k) == 0.0);

    for (int i = 0; i < 8; ++i)
        c.record(k, crawler::ProbeOutcome::Success, std::chrono::milliseconds(20));

    const auto now = std::chrono::system_clock::now();
    const double c0 = c.confidence(k, now);
    const double n0 = c.effective_observations(k, now);
    assert(n0 > 7.0 && n0 <= 8.0);
    assert(c0 > 0.0 && c0 < 1.0);

    const double stale = c.confidence(k, now + std::chrono::hours(1));
    assert(stale < c0);

    crawler::AdaptiveCapacityController mixed(cfg);
    for (int i = 0; i < 8; ++i)
        mixed.record(k, i < 4 ? crawler::ProbeOutcome::Success : crawler::ProbeOutcome::Failure);
    // Same evidence quantity => confidence is comparable regardless of outcome polarity.
    const double mixed_conf = mixed.confidence(k, std::chrono::system_clock::now());
    assert(std::fabs(mixed_conf - c0) < 0.08);

    std::cout << "phase1016 confidence tests: PASS\n";
}
