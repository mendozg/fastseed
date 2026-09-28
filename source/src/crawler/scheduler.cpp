#include "fastseed/crawler/scheduler.hpp"
#include "fastseed/crawler/adaptive_feedback.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

namespace fastseed::crawler {

PeerCandidate HealthAwareScheduler::candidate_from_health(const health::PeerHealth& peer) {
    PeerCandidate c;
    c.host = peer.host;
    c.port = peer.port;
    c.source = CandidateSource::PersistentDatabase;
    c.network = peer.network;
    c.address_bytes = peer.address_bytes;
    c.state = (peer.network == CandidateNetwork::TorV3 || peer.network == CandidateNetwork::I2P)
        ? CandidateState::AwaitingTransport : CandidateState::Queued;
    c.attempts = static_cast<std::uint32_t>(std::min<std::uint64_t>(peer.observations, std::numeric_limits<std::uint32_t>::max()));
    c.successes = static_cast<std::uint32_t>(std::min<std::uint64_t>(peer.successful_probes, std::numeric_limits<std::uint32_t>::max()));
    c.failures = static_cast<std::uint32_t>(std::min<std::uint64_t>(peer.failed_probes, std::numeric_limits<std::uint32_t>::max()));
    c.last_error = peer.last_error;
    return c;
}

int HealthAwareScheduler::priority(const health::PeerHealth& peer,
                                   std::chrono::system_clock::time_point now) {
    if (peer.state == health::PeerState::Quarantined && peer.quarantine_until > now) return -1000;
    if (peer.network == CandidateNetwork::TorV3 || peer.network == CandidateNetwork::I2P) {
        if (peer.state == health::PeerState::Hot || peer.state == health::PeerState::Warm) return 60;
        return 40;
    }
    switch (peer.state) {
    case health::PeerState::Hot: return 100;
    case health::PeerState::Warm: return 90;
    case health::PeerState::Validated: return 80;
    case health::PeerState::Cold: return 50;
    case health::PeerState::Candidate: return 40;
    case health::PeerState::Discovered: return 30;
    case health::PeerState::Cooldown: return 20;
    case health::PeerState::Quarantined: return 0;
    case health::PeerState::Probing: return 0;
    }
    return 0;
}

std::vector<ScheduledPeer> HealthAwareScheduler::select_due(
    const std::vector<health::PeerHealth>& peers,
    std::chrono::system_clock::time_point now) const {
    std::vector<ScheduledPeer> out;
    out.reserve(std::min(config_.max_batch, peers.size()));
    for (const auto& peer : peers) {
        if (peer.host.empty() || peer.port == 0) continue;
        if (peer.state == health::PeerState::Quarantined && peer.quarantine_until > now) continue;
        if (peer.network == CandidateNetwork::Unknown) continue;
        const auto due = peer.next_probe == std::chrono::system_clock::time_point{}
            ? now : peer.next_probe;
        if (due > now) continue;
        auto candidate = candidate_from_health(peer);
        candidate.state = (peer.network == CandidateNetwork::TorV3 || peer.network == CandidateNetwork::I2P)
            ? CandidateState::AwaitingTransport : CandidateState::Queued;
        out.push_back({std::move(candidate), due, priority(peer, now)});
    }
    std::stable_sort(out.begin(), out.end(), [](const ScheduledPeer& a, const ScheduledPeer& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        if (a.due != b.due) return a.due < b.due;
        return a.candidate.key() < b.candidate.key();
    });
    if (out.size() > config_.max_batch) out.resize(config_.max_batch);
    return out;
}

} // namespace fastseed::crawler

namespace fastseed::crawler {

void ExplorationTelemetry::begin_cycle(std::size_t budget) noexcept {
    cycles_.fetch_add(1, std::memory_order_relaxed);
    budget_.fetch_add(static_cast<std::uint64_t>(budget), std::memory_order_relaxed);
}
void ExplorationTelemetry::allocate(transport::TransportKind kind) noexcept {
    allocated_[index(kind)].fetch_add(1, std::memory_order_relaxed);
}
void ExplorationTelemetry::opportunity(transport::TransportKind kind) noexcept {
    opportunities_[index(kind)].fetch_add(1, std::memory_order_relaxed);
}
void ExplorationTelemetry::selected(transport::TransportKind kind) noexcept {
    selected_[index(kind)].fetch_add(1, std::memory_order_relaxed);
}
void ExplorationTelemetry::quota_rejection(transport::TransportKind kind) noexcept {
    quota_rejections_[index(kind)].fetch_add(1, std::memory_order_relaxed);
}
void ExplorationTelemetry::starvation(transport::TransportKind kind) noexcept {
    starvation_[index(kind)].fetch_add(1, std::memory_order_relaxed);
}
void ExplorationTelemetry::exploration_used() noexcept {
    used_.fetch_add(1, std::memory_order_relaxed);
}
ExplorationTelemetrySnapshot ExplorationTelemetry::snapshot() const noexcept {
    ExplorationTelemetrySnapshot out;
    out.scheduling_cycles = cycles_.load(std::memory_order_relaxed);
    out.exploration_budget = budget_.load(std::memory_order_relaxed);
    out.exploration_used = used_.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < 3; ++i) {
        out.transport[i] = {
            allocated_[i].load(std::memory_order_relaxed),
            opportunities_[i].load(std::memory_order_relaxed),
            selected_[i].load(std::memory_order_relaxed),
            quota_rejections_[i].load(std::memory_order_relaxed),
            starvation_[i].load(std::memory_order_relaxed)};
    }
    return out;
}

transport::TransportKind AdaptiveProbeScheduler::transport_for(const health::PeerHealth& peer) noexcept {
    switch (peer.network) {
    case CandidateNetwork::TorV3: return transport::TransportKind::TorSocks5;
    case CandidateNetwork::I2P: return transport::TransportKind::I2pSam;
    default: return transport::TransportKind::DirectTcp;
    }
}

NetworkQuota AdaptiveProbeScheduler::quota_for(transport::TransportKind kind) const noexcept {
    switch (kind) {
    case transport::TransportKind::TorSocks5: return config_.tor_socks5;
    case transport::TransportKind::I2pSam: return config_.i2p_sam;
    case transport::TransportKind::DirectTcp: return config_.direct_tcp;
    }
    return config_.direct_tcp;
}

std::size_t AdaptiveProbeScheduler::active_for(transport::TransportKind kind,
                                                const ActiveProbeCounts& active) const noexcept {
    switch (kind) {
    case transport::TransportKind::TorSocks5: return active.tor_socks5;
    case transport::TransportKind::I2pSam: return active.i2p_sam;
    case transport::TransportKind::DirectTcp: return active.direct_tcp;
    }
    return active.direct_tcp;
}

std::chrono::seconds AdaptiveProbeScheduler::recommended_refresh(const health::PeerHealth& peer) const noexcept {
    auto base = config_.stale_refresh;
    switch (peer.state) {
    case health::PeerState::Hot: base = config_.hot_refresh; break;
    case health::PeerState::Warm: base = config_.warm_refresh; break;
    case health::PeerState::Cold: base = config_.cold_refresh; break;
    default: break;
    }

    const double availability = peer.availability();
    if (peer.successful_probes + peer.failed_probes >= 3) {
        if (availability >= config_.high_availability && peer.latency_avg <= config_.high_latency) {
            base = std::min(config_.maximum_refresh, base * 2);
        } else if (availability <= config_.low_availability || peer.latency_avg >= config_.low_latency) {
            base = std::max(config_.minimum_refresh, base / 2);
        }
    }
    return std::clamp(base, config_.minimum_refresh, config_.maximum_refresh);
}

std::chrono::system_clock::time_point AdaptiveProbeScheduler::effective_due(
    const health::PeerHealth& peer, std::chrono::system_clock::time_point now) const noexcept {
    if (peer.last_failure > peer.last_success && peer.next_probe != std::chrono::system_clock::time_point{})
        return peer.next_probe;
    if (peer.last_success != std::chrono::system_clock::time_point{})
        return peer.last_success + recommended_refresh(peer);
    return peer.next_probe == std::chrono::system_clock::time_point{} ? now : peer.next_probe;
}

std::vector<ScheduledPeer> AdaptiveProbeScheduler::select_due(
    const std::vector<health::PeerHealth>& peers,
    ActiveProbeCounts active,
    std::chrono::system_clock::time_point now) const {
    std::vector<ScheduledPeer> candidates;
    candidates.reserve(peers.size());
    for (const auto& peer : peers) {
        if (peer.host.empty() || peer.port == 0 || peer.network == CandidateNetwork::Unknown) continue;
        if (peer.state == health::PeerState::Quarantined && peer.quarantine_until > now) continue;
        const auto due = effective_due(peer, now);
        if (due > now) continue;
        candidates.push_back({HealthAwareScheduler::candidate_from_health(peer), due,
                              HealthAwareScheduler::priority(peer, now)});
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const ScheduledPeer& a, const ScheduledPeer& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        if (a.due != b.due) return a.due < b.due;
        return a.candidate.key() < b.candidate.key();
    });

    std::vector<ScheduledPeer> out;
    out.reserve(std::min(config_.max_batch, candidates.size()));
    std::size_t direct = active.direct_tcp, tor = active.tor_socks5, i2p = active.i2p_sam;
    for (auto& item : candidates) {
        if (out.size() >= config_.max_batch) break;
        const auto tk = item.candidate.network == CandidateNetwork::TorV3 ? transport::TransportKind::TorSocks5 :
                       item.candidate.network == CandidateNetwork::I2P ? transport::TransportKind::I2pSam :
                       transport::TransportKind::DirectTcp;
        const auto quota = quota_for(tk);
        auto& used = tk == transport::TransportKind::TorSocks5 ? tor :
                     tk == transport::TransportKind::I2pSam ? i2p : direct;
        if (used >= quota.max_inflight) continue;
        ++used;
        out.push_back(std::move(item));
    }
    return out;
}

std::vector<ScheduledPeer> AdaptiveProbeScheduler::select_due(
    const std::vector<health::PeerHealth>& peers,
    const AdaptiveCapacityController& controller,
    ActiveProbeCounts active,
    std::chrono::system_clock::time_point now) const {
    struct Ranked { ScheduledPeer item; transport::TransportKind kind; double confidence; };
    std::vector<Ranked> candidates;
    candidates.reserve(peers.size());

    for (const auto& peer : peers) {
        if (peer.host.empty() || peer.port == 0 || peer.network == CandidateNetwork::Unknown) continue;
        if (peer.state == health::PeerState::Quarantined && peer.quarantine_until > now) continue;
        const auto due = effective_due(peer, now);
        if (due > now) continue;
        const auto kind = transport_for(peer);
        const double conf = controller.confidence(kind, now);
        int p = HealthAwareScheduler::priority(peer, now);
        // Bounded exploration: unknown/low-confidence transports get up to
        // 15 points, while high-confidence transports get no bonus.
        const int exploration = static_cast<int>(std::lround((1.0 - conf) * 15.0));
        p += std::clamp(exploration, 0, 15);
        candidates.push_back({{HealthAwareScheduler::candidate_from_health(peer), due, p}, kind, conf});
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const Ranked& a, const Ranked& b) {
        if (a.item.priority != b.item.priority) return a.item.priority > b.item.priority;
        if (a.item.due != b.item.due) return a.item.due < b.item.due;
        if (a.confidence != b.confidence) return a.confidence < b.confidence;
        return a.item.candidate.key() < b.item.candidate.key();
    });

    std::vector<ScheduledPeer> out;
    out.reserve(std::min(config_.max_batch, candidates.size()));
    std::size_t direct = active.direct_tcp, tor = active.tor_socks5, i2p = active.i2p_sam;
    const std::size_t initial_total = direct + tor + i2p;
    const std::size_t global_limit = std::min<std::size_t>(config_.max_batch,
        controller.total_capacity());
    const std::size_t available = global_limit > initial_total ? global_limit - initial_total : 0;

    // Reserve only a bounded portion of each scheduling batch for exploration.
    // This prevents established transports from being starved while still giving
    // low-confidence transports regular opportunities to prove that conditions changed.
    const double fraction = std::clamp(config_.exploration_fraction, 0.0, 1.0);
    std::size_t exploration_budget = static_cast<std::size_t>(std::ceil(
        static_cast<double>(available) * fraction));
    if (available > 0 && fraction > 0.0)
        exploration_budget = std::max(exploration_budget, config_.minimum_exploration);
    exploration_budget = std::min(exploration_budget, config_.maximum_exploration);
    exploration_budget = std::min(exploration_budget, available);

    auto try_add = [&](Ranked& ranked) -> bool {
        auto& used = ranked.kind == transport::TransportKind::TorSocks5 ? tor :
                     ranked.kind == transport::TransportKind::I2pSam ? i2p : direct;
        if (used >= quota_for(ranked.kind).max_inflight) return false;
        if (!controller.admit(ranked.kind, used)) return false;
        ++used;
        out.push_back(std::move(ranked.item));
        return true;
    };

    // Exploration is transport-fair rather than candidate-order driven. First
    // give each eligible low-confidence transport at most one slot, beginning
    // from a rotating transport so a small budget (e.g. 2/3) cannot permanently
    // starve the transport that happens to sort last. Remaining exploration slots
    // are then distributed round-robin. The rotation is derived from wall-clock
    // time, so the const scheduler remains stateless/thread-safe.
    telemetry_.begin_cycle(exploration_budget);
    std::size_t explored = 0;
    std::vector<bool> selected(candidates.size(), false);
    const transport::TransportKind kinds[] = {
        transport::TransportKind::DirectTcp,
        transport::TransportKind::TorSocks5,
        transport::TransportKind::I2pSam};
    const auto rotation_seconds = std::max<std::int64_t>(1, config_.exploration_rotation.count());
    const auto epoch_seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    const std::size_t rotation = static_cast<std::size_t>((epoch_seconds / rotation_seconds) % 3);

    auto explore_transport = [&](transport::TransportKind kind) -> bool {
        telemetry_.allocate(kind);
        bool eligible = false;
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            if (selected[i] || candidates[i].confidence >= config_.low_confidence_threshold ||
                candidates[i].kind != kind) continue;
            eligible = true;
            telemetry_.opportunity(kind);
            if (try_add(candidates[i])) {
                selected[i] = true; ++explored;
                telemetry_.selected(kind);
                telemetry_.exploration_used();
                return true;
            }
            telemetry_.quota_rejection(kind);
        }
        if (eligible) telemetry_.starvation(kind);
        return false;
    };

    // One fair pass: each transport gets one opportunity before any transport
    // can consume a second exploration slot.
    for (std::size_t offset = 0; offset < 3 && explored < exploration_budget; ++offset)
        explore_transport(kinds[(rotation + offset) % 3]);

    // If the budget exceeds the number of transports, distribute the remainder
    // round-robin while respecting each transport's own quota.
    for (std::size_t offset = 0; explored < exploration_budget && offset < 3 * exploration_budget; ++offset)
        explore_transport(kinds[(rotation + offset) % 3]);

    // Exploitation pass: established/high-confidence candidates get the first
    // opportunity for every slot not consumed by exploration.
    for (std::size_t i = 0; i < candidates.size() && out.size() < available; ++i) {
        if (selected[i] || candidates[i].confidence < config_.low_confidence_threshold) continue;
        if (try_add(candidates[i])) selected[i] = true;
    }

    // If there is no established work available, use otherwise-idle capacity for
    // additional exploration. This keeps the network busy without letting a large
    // low-confidence pool crowd out established peers.
    for (std::size_t i = 0; i < candidates.size() && out.size() < available; ++i) {
        if (selected[i]) continue;
        if (try_add(candidates[i])) selected[i] = true;
    }
    return out;
}

} // namespace fastseed::crawler
