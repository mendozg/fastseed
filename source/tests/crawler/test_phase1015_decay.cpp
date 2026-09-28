#include "fastseed/crawler/adaptive_feedback.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

int main() {
    using namespace fastseed;
    crawler::FeedbackConfig cfg;
    cfg.learning_half_life = std::chrono::hours(1);
    cfg.capacity_recovery_period = std::chrono::hours(4);
    crawler::AdaptiveCapacityController c(cfg);
    const auto kind = transport::TransportKind::DirectTcp;

    for (int i=0; i<8; ++i)
        c.record(kind, crawler::ProbeOutcome::Success, std::chrono::milliseconds(20));
    const auto before = c.stats(kind);
    const auto learned = c.capacity(kind);
    assert(before.attempts == 8);
    assert(learned >= cfg.direct_tcp.target);

    auto now = std::chrono::system_clock::now() + std::chrono::hours(2);
    assert(c.decay(kind, now));
    auto after = c.stats(kind);
    assert(after.attempts < before.attempts);
    assert(after.successes < before.successes);
    assert(after.consecutive_successes == 0);
    assert(c.capacity(kind) <= learned);

    auto stable = c.stats(kind);
    assert(!c.decay(kind, now));
    assert(c.stats(kind).attempts == stable.attempts);

    crawler::PersistedTransportState p = c.snapshot(kind);
    assert(p.updated_at_ms > 0);
    std::cout << "phase1015 decay tests: PASS\
";
}
