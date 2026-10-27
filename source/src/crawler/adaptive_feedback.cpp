#include "fastseed/crawler/adaptive_feedback.hpp"
#include <algorithm>
#include <limits>
#include <cmath>

namespace fastseed::crawler {

AdaptiveCapacityController::AdaptiveCapacityController(FeedbackConfig config) : config_(config) {
    direct_.capacity = config_.direct_tcp.target;
    tor_.capacity = config_.tor_socks5.target;
    i2p_.capacity = config_.i2p_sam.target;
}

AdaptiveCapacityController::State& AdaptiveCapacityController::state(transport::TransportKind kind) noexcept {
    switch (kind) { case transport::TransportKind::TorSocks5: return tor_; case transport::TransportKind::I2pSam: return i2p_; default: return direct_; }
}
const AdaptiveCapacityController::State& AdaptiveCapacityController::state(transport::TransportKind kind) const noexcept {
    switch (kind) { case transport::TransportKind::TorSocks5: return tor_; case transport::TransportKind::I2pSam: return i2p_; default: return direct_; }
}
TransportCapacity AdaptiveCapacityController::limits(transport::TransportKind kind) const noexcept {
    switch (kind) { case transport::TransportKind::TorSocks5: return config_.tor_socks5; case transport::TransportKind::I2pSam: return config_.i2p_sam; default: return config_.direct_tcp; }
}

void AdaptiveCapacityController::record(transport::TransportKind kind, ProbeOutcome outcome, std::chrono::milliseconds latency) {
    const auto now = std::chrono::system_clock::now();
    decay(kind, now);
    auto& s = state(kind); auto& x = s.stats; ++x.attempts;
    s.evidence_updated = now;
    if (latency.count() > 0) {
        if (x.latency_avg.count() == 0) x.latency_avg = latency;
        else x.latency_avg = std::chrono::milliseconds((x.latency_avg.count() * 7 + latency.count()) / 8);
    }
    switch (outcome) {
    case ProbeOutcome::Success: ++x.successes; ++x.consecutive_successes; x.consecutive_failures = 0; break;
    case ProbeOutcome::Failure: ++x.failures; ++x.consecutive_failures; x.consecutive_successes = 0; break;
    case ProbeOutcome::Timeout: ++x.failures; ++x.timeouts; ++x.consecutive_failures; x.consecutive_successes = 0; break;
    case ProbeOutcome::HandshakeFailure: ++x.failures; ++x.handshake_failures; ++x.consecutive_failures; x.consecutive_successes = 0; break;
    case ProbeOutcome::Unsupported: ++x.unsupported; x.consecutive_successes = 0; break;
    }
    maybe_adjust(kind, s);
}

void AdaptiveCapacityController::maybe_adjust(transport::TransportKind kind, State& s) {
    const auto now = std::chrono::system_clock::now();
    if (s.last_adjustment != std::chrono::system_clock::time_point{} && now - s.last_adjustment < config_.cooldown_after_adjustment) return;
    auto lim = limits(kind); auto& x = s.stats;
    const auto observed = x.successes + x.failures;
    if (observed < 4) return;
    const double rate = observed ? static_cast<double>(x.successes) / static_cast<double>(observed) : 0.0;
    std::size_t next = s.capacity;
    if (x.consecutive_failures >= config_.failure_streak_for_decrease || rate <= config_.decrease_success_rate || x.latency_avg.count() >= config_.decrease_latency_ms) {
        next = s.capacity > config_.adjustment_step ? s.capacity - config_.adjustment_step : lim.minimum;
    } else if (rate >= config_.increase_success_rate && x.latency_avg.count() > 0 && x.latency_avg.count() <= config_.increase_latency_ms) {
        next = std::min(lim.maximum, s.capacity + config_.adjustment_step);
    }
    next = std::clamp(next, lim.minimum, lim.maximum);
    if (next != s.capacity) { s.capacity = next; s.last_adjustment = now; }
}

bool AdaptiveCapacityController::decay(transport::TransportKind kind,
                                       std::chrono::system_clock::time_point now) noexcept {
    auto& s = state(kind);
    if (s.evidence_updated == std::chrono::system_clock::time_point{}) return false;
    const auto age = now - s.evidence_updated;
    if (age <= config_.learning_half_life) return false;
    if (config_.learning_half_life.count() <= 0) return false;

    const double age_hours = std::chrono::duration<double, std::ratio<3600>>(age).count();
    const double half_lives = age_hours /
        static_cast<double>(config_.learning_half_life.count());
    const double factor = std::pow(0.5, half_lives);
    auto decay_u64 = [factor](std::uint64_t v) -> std::uint64_t {
        return static_cast<std::uint64_t>(std::llround(static_cast<double>(v) * factor));
    };
    auto& x = s.stats;
    x.attempts = decay_u64(x.attempts);
    x.successes = decay_u64(x.successes);
    x.failures = decay_u64(x.failures);
    x.timeouts = decay_u64(x.timeouts);
    x.handshake_failures = decay_u64(x.handshake_failures);
    x.unsupported = decay_u64(x.unsupported);
    x.consecutive_failures = 0;
    x.consecutive_successes = 0;

    const auto lim = limits(kind);
    const auto target = lim.target;
    const double recovery = std::min(1.0, age_hours /
        static_cast<double>(std::max<std::int64_t>(1, config_.capacity_recovery_period.count())));
    const double next = static_cast<double>(s.capacity) +
        (static_cast<double>(target) - static_cast<double>(s.capacity)) * recovery;
    s.capacity = std::clamp(static_cast<std::size_t>(std::llround(next)), lim.minimum, lim.maximum);
    s.evidence_updated = now;
    return true;
}

double AdaptiveCapacityController::effective_observations(transport::TransportKind kind,
        std::chrono::system_clock::time_point now) const noexcept {
    const auto& s = state(kind);
    const auto& x = s.stats;
    const double observations = static_cast<double>(x.successes + x.failures);
    if (observations <= 0.0) return 0.0;
    if (s.evidence_updated == std::chrono::system_clock::time_point{} ||
        config_.learning_half_life.count() <= 0) return observations;
    const auto age = now > s.evidence_updated ? now - s.evidence_updated : std::chrono::system_clock::duration::zero();
    const double half_lives = std::chrono::duration<double, std::ratio<3600>>(age).count() /
        static_cast<double>(config_.learning_half_life.count());
    return observations * std::pow(0.5, std::max(0.0, half_lives));
}

double AdaptiveCapacityController::confidence(transport::TransportKind kind,
        std::chrono::system_clock::time_point now) const noexcept {
    const double n = effective_observations(kind, now);
    if (n <= 0.0) return 0.0;
    // Confidence represents evidence quantity/freshness only; it does not encode success quality.
    return std::clamp(1.0 - std::exp(-n / 8.0), 0.0, 1.0);
}

std::size_t AdaptiveCapacityController::capacity(transport::TransportKind kind) const noexcept { return state(kind).capacity; }
TransportFeedbackStats AdaptiveCapacityController::stats(transport::TransportKind kind) const noexcept { return state(kind).stats; }
bool AdaptiveCapacityController::admit(transport::TransportKind kind, std::size_t active) const noexcept { return active < capacity(kind); }
std::size_t AdaptiveCapacityController::total_capacity() const noexcept { return std::min(config_.global_max_inflight, direct_.capacity + tor_.capacity + i2p_.capacity); }
void AdaptiveCapacityController::reset(transport::TransportKind kind) noexcept { auto& s=state(kind); s.stats={}; s.capacity=limits(kind).target; s.last_adjustment={}; s.evidence_updated={}; }

} // namespace fastseed::crawler


namespace fastseed::crawler {

PersistedTransportState AdaptiveCapacityController::snapshot(transport::TransportKind kind) const noexcept {
    const auto& s = state(kind);
    PersistedTransportState out;
    out.attempts = s.stats.attempts; out.successes = s.stats.successes; out.failures = s.stats.failures;
    out.timeouts = s.stats.timeouts; out.handshake_failures = s.stats.handshake_failures;
    out.unsupported = s.stats.unsupported; out.consecutive_failures = s.stats.consecutive_failures;
    out.consecutive_successes = s.stats.consecutive_successes; out.latency_avg_ms = s.stats.latency_avg.count();
    out.learned_capacity = s.capacity;
    if (s.evidence_updated != std::chrono::system_clock::time_point{})
        out.updated_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(s.evidence_updated.time_since_epoch()).count();
    return out;
}

bool AdaptiveCapacityController::restore(transport::TransportKind kind, const PersistedTransportState& p,
                                         std::chrono::system_clock::time_point now) noexcept {
    if (p.state_version != 1 || p.learned_capacity == 0 || p.updated_at_ms <= 0) return false;
    const auto lim = limits(kind);
    if (p.learned_capacity < lim.minimum || p.learned_capacity > lim.maximum) return false;
    const auto updated = std::chrono::system_clock::time_point{std::chrono::milliseconds(p.updated_at_ms)};
    if (updated > now || now - updated > std::chrono::hours(24 * 7)) return false;
    auto& s = state(kind);
    s.stats.attempts = p.attempts; s.stats.successes = p.successes; s.stats.failures = p.failures;
    s.stats.timeouts = p.timeouts; s.stats.handshake_failures = p.handshake_failures; s.stats.unsupported = p.unsupported;
    s.stats.consecutive_failures = p.consecutive_failures; s.stats.consecutive_successes = p.consecutive_successes;
    s.stats.latency_avg = std::chrono::milliseconds(std::max<std::int64_t>(0, p.latency_avg_ms));
    s.capacity = p.learned_capacity; s.last_adjustment = updated; s.evidence_updated = updated;
    return true;
}

} // namespace fastseed::crawler
