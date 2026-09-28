#include "fastseed/health/peer_health.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace fastseed::health {
namespace {

transport::TransportKind transport_for_network(crawler::CandidateNetwork network, const std::string& host) noexcept {
    switch (network) {
    case crawler::CandidateNetwork::TorV3: return transport::TransportKind::TorSocks5;
    case crawler::CandidateNetwork::I2P: return transport::TransportKind::I2pSam;
    case crawler::CandidateNetwork::IPv4:
    case crawler::CandidateNetwork::IPv6: return transport::TransportKind::DirectTcp;
    case crawler::CandidateNetwork::Unknown:
        return host.find(':') != std::string::npos
            ? transport::TransportKind::DirectTcp : transport::TransportKind::DirectTcp;
    }
    return transport::TransportKind::DirectTcp;
}

std::chrono::seconds failure_cooldown(const HealthConfig& config, std::uint32_t failures) noexcept {
    if (failures == 0) return config.cooldown_initial;
    double seconds = static_cast<double>(config.cooldown_initial.count());
    for (std::uint32_t i = 1; i < failures && seconds < static_cast<double>(config.cooldown_max.count()); ++i)
        seconds *= config.cooldown_multiplier;
    seconds = std::min(seconds, static_cast<double>(config.cooldown_max.count()));
    return std::chrono::seconds(static_cast<std::int64_t>(seconds));
}

} // namespace

std::string_view to_string(PeerState state) noexcept {
    switch (state) {
    case PeerState::Discovered: return "DISCOVERED";
    case PeerState::Candidate: return "CANDIDATE";
    case PeerState::Probing: return "PROBING";
    case PeerState::Validated: return "VALIDATED";
    case PeerState::Warm: return "WARM";
    case PeerState::Hot: return "HOT";
    case PeerState::Cold: return "COLD";
    case PeerState::Cooldown: return "COOLDOWN";
    case PeerState::Quarantined: return "QUARANTINED";
    }
    return "UNKNOWN";
}

std::string PeerHealth::key() const {
    return host + ":" + std::to_string(port);
}

bool PeerHealth::fresh(std::chrono::system_clock::time_point now, std::chrono::seconds max_age) const noexcept {
    if (last_success.time_since_epoch().count() == 0 || now < last_success) return false;
    return now - last_success <= max_age;
}

double PeerHealth::availability() const noexcept {
    const auto total = successful_probes + failed_probes;
    return total == 0 ? 0.0 : static_cast<double>(successful_probes) / static_cast<double>(total);
}

PeerHealthCache::PeerHealthCache(HealthConfig config) : config_(std::move(config)) {
    if (config_.minimum_observations_for_hot == 0) config_.minimum_observations_for_hot = 1;
    if (config_.latency_history_size == 0) config_.latency_history_size = 1;
}

PeerHealth& PeerHealthCache::get_or_create(const crawler::PeerCandidate& candidate,
                                            std::chrono::system_clock::time_point now) {
    auto [it, inserted] = peers_.try_emplace(candidate.key());
    auto& peer = it->second;
    if (inserted) {
        peer.host = candidate.host;
        peer.port = candidate.port;
        peer.source = candidate.source;
        peer.network = candidate.network;
        peer.state = PeerState::Candidate;
        peer.first_seen = now;
    } else if (peer.source != candidate.source) {
        peer.source = candidate.source;
    }
    peer.network = candidate.network;
    if (peer.network == crawler::CandidateNetwork::Unknown) {
        peer.network = candidate.host.find(':') != std::string::npos
            ? crawler::CandidateNetwork::IPv6
            : crawler::CandidateNetwork::IPv4;
    }
    peer.address_bytes = candidate.address_bytes;
    peer.transport = transport_for_network(peer.network, peer.host);
    peer.last_seen = now;
    return peer;
}

void PeerHealthCache::observe_discovered(const crawler::PeerCandidate& candidate) {
    const auto now = std::chrono::system_clock::now();
    auto& peer = get_or_create(candidate, now);
    ++peer.observations;
    peer.state = candidate.transport_probe_supported() ? PeerState::Candidate : PeerState::Candidate;
    peer.last_error.clear();
    trim();
}

void PeerHealthCache::observe_success(const crawler::PeerCandidate& candidate,
                                      const protocol::ProbeResult& result) {
    const auto now = std::chrono::system_clock::now();
    auto& peer = get_or_create(candidate, now);
    ++peer.observations;
    ++peer.successful_probes;
    ++peer.transport_attempts;
    ++peer.transport_successes;
    ++peer.consecutive_successes;
    peer.consecutive_failures = 0;
    peer.last_success = now;
    peer.last_error.clear();
    peer.transport_last_error.clear();
    peer.next_probe = now + config_.freshness;
    peer.protocol_version = result.protocol_version;
    peer.services = result.services;
    peer.user_agent = result.user_agent;
    peer.latency_last = result.handshake_time.count() > 0 ? result.handshake_time : result.connect_time;
    if (peer.latency_last.count() < 0) peer.latency_last = {};
    peer.latency_history.push_back(peer.latency_last);
    if (peer.latency_history.size() > config_.latency_history_size)
        peer.latency_history.erase(peer.latency_history.begin());

    peer.state = PeerState::Validated;
    if (peer.successful_probes >= config_.minimum_observations_for_hot &&
        peer.consecutive_successes >= config_.minimum_observations_for_hot) {
        peer.state = PeerState::Hot;
    } else {
        peer.state = PeerState::Warm;
    }
    peer.quarantine_until = {};
    recalculate(peer);
    trim();
}

void PeerHealthCache::observe_failure(const crawler::PeerCandidate& candidate,
                                      const protocol::ProbeResult& result) {
    const auto now = std::chrono::system_clock::now();
    auto& peer = get_or_create(candidate, now);
    ++peer.observations;
    ++peer.failed_probes;
    ++peer.transport_attempts;
    ++peer.transport_failures;
    ++peer.consecutive_failures;
    peer.consecutive_successes = 0;
    peer.last_failure = now;
    peer.last_error = result.error;
    peer.transport_last_error = result.error;
    peer.next_probe = now + failure_cooldown(config_, peer.consecutive_failures);

    if (peer.consecutive_failures >= config_.quarantine_after_failures) {
        peer.state = PeerState::Quarantined;
        peer.quarantine_until = now + config_.quarantine;
    } else if (peer.successful_probes > 0) {
        peer.state = PeerState::Cold;
    } else {
        peer.state = PeerState::Cooldown;
    }
    recalculate(peer);
    trim();
}

void PeerHealthCache::recalculate(PeerHealth& peer) {
    if (peer.latency_history.empty()) {
        peer.latency_avg = {};
        peer.latency_p95 = {};
        return;
    }
    const auto sum = std::accumulate(peer.latency_history.begin(), peer.latency_history.end(),
                                     std::int64_t{0}, [](std::int64_t acc, auto v) {
                                         return acc + v.count();
                                     });
    peer.latency_avg = std::chrono::milliseconds(sum / static_cast<std::int64_t>(peer.latency_history.size()));
    auto sorted = peer.latency_history;
    std::sort(sorted.begin(), sorted.end());
    const auto index = static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(sorted.size()))) - 1;
    peer.latency_p95 = sorted[std::min(index, sorted.size() - 1)];
}

bool PeerHealthCache::contains(const std::string& key) const { return peers_.find(key) != peers_.end(); }

const PeerHealth* PeerHealthCache::find(const std::string& key) const noexcept {
    auto it = peers_.find(key);
    return it == peers_.end() ? nullptr : &it->second;
}

std::vector<PeerHealth> PeerHealthCache::snapshot() const {
    std::vector<PeerHealth> out;
    out.reserve(peers_.size());
    for (const auto& [key, peer] : peers_) out.push_back(peer);
    return out;
}

std::vector<PeerHealth> PeerHealthCache::hot_snapshot() const {
    std::vector<PeerHealth> out;
    for (const auto& [key, peer] : peers_)
        if (peer.state == PeerState::Hot) out.push_back(peer);
    return out;
}

std::vector<PeerHealth> PeerHealthCache::dns_snapshot(
    std::size_t max_results, bool include_warm,
    std::chrono::system_clock::time_point now) const {
    std::vector<PeerHealth> candidates;
    candidates.reserve(peers_.size());
    for (const auto& [key, peer] : peers_) {
        const bool network_eligible = peer.network == crawler::CandidateNetwork::IPv4 ||
            peer.network == crawler::CandidateNetwork::IPv6;
        const bool eligible_state = network_eligible && (peer.state == PeerState::Hot ||
            (include_warm && peer.state == PeerState::Warm));
        if (eligible_state && peer.fresh(now, config_.freshness)) candidates.push_back(peer);
    }
    std::sort(candidates.begin(), candidates.end(), [](const PeerHealth& a, const PeerHealth& b) {
        if (a.state != b.state) return a.state == PeerState::Hot;
        if (a.last_success != b.last_success) return a.last_success > b.last_success;
        return a.key() < b.key();
    });
    if (candidates.size() > max_results) candidates.resize(max_results);
    return candidates;
}

HealthStats PeerHealthCache::stats() const noexcept {
    HealthStats s{};
    s.total = peers_.size();
    for (const auto& [key, peer] : peers_) {
        switch (peer.state) {
        case PeerState::Hot:
            if (peer.network == crawler::CandidateNetwork::IPv4) ++s.hot_ipv4;
            else if (peer.network == crawler::CandidateNetwork::IPv6) ++s.hot_ipv6;
            else if (peer.network == crawler::CandidateNetwork::TorV3) ++s.hot_torv3;
            else if (peer.network == crawler::CandidateNetwork::I2P) ++s.hot_i2p;
            break;
        case PeerState::Warm: ++s.warm; break;
        case PeerState::Cold: ++s.cold; break;
        case PeerState::Quarantined: ++s.quarantined; break;
        default: break;
        }
        s.observations += peer.observations;
        s.successful_probes += peer.successful_probes;
        s.failed_probes += peer.failed_probes;
    }
    return s;
}

void PeerHealthCache::prune(std::chrono::system_clock::time_point now) {
    for (auto it = peers_.begin(); it != peers_.end();) {
        auto& peer = it->second;
        if (peer.state == PeerState::Quarantined && peer.quarantine_until <= now) {
            peer.state = PeerState::Cold;
            peer.quarantine_until = {};
        }
        ++it;
    }
    trim();
}

void PeerHealthCache::trim() {
    auto count_state = [this](PeerState state) {
        return static_cast<std::size_t>(std::count_if(peers_.begin(), peers_.end(),
            [state](const auto& p) { return p.second.state == state; }));
    };

    auto evict_oldest = [this, &count_state](PeerState state, std::size_t limit) {
        while (count_state(state) > limit) {
            auto it = std::min_element(peers_.begin(), peers_.end(), [state](const auto& a, const auto& b) {
                const bool aa = a.second.state == state;
                const bool bb = b.second.state == state;
                if (aa != bb) return aa;
                return a.second.last_success < b.second.last_success;
            });
            if (it == peers_.end() || it->second.state != state) break;
            peers_.erase(it);
        }
    };

    evict_oldest(PeerState::Hot, config_.hot_ipv4 + config_.hot_ipv6);
    evict_oldest(PeerState::Warm, config_.warm_max);
    evict_oldest(PeerState::Cold, config_.cold_max);
}

void PeerHealthCache::restore(const PeerHealth& peer) {
    if (peer.host.empty() || peer.port == 0) return;
    peers_[peer.key()] = peer;
    trim();
}

void PeerHealthCache::restore(const std::vector<PeerHealth>& peers) {
    for (const auto& peer : peers) restore(peer);
}

void PeerHealthCache::clear() { peers_.clear(); }

} // namespace fastseed::health
