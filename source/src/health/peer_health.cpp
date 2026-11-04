#include "fastseed/health/peer_health.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <limits>
#include <tuple>

namespace fastseed::health {
namespace {

template<class T> void increment(T& value) { if(value<std::numeric_limits<T>::max()) ++value; }
void add_saturated(std::uint64_t& value,std::uint64_t other) {
    value += std::min(other,std::numeric_limits<std::uint64_t>::max()-value);
}
int retention_rank(PeerState state) {
    switch(state) {
    case PeerState::Hot:return 5;case PeerState::Warm:return 4;case PeerState::Validated:return 3;
    case PeerState::Cold:return 2;case PeerState::Quarantined:case PeerState::Cooldown:return 1;default:return 0;
    }
}

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
    const auto initial=std::max<std::int64_t>(0,config.cooldown_initial.count());
    const auto maximum=std::max<std::int64_t>(0,config.cooldown_max.count());
    if(initial==0 || maximum==0)return std::chrono::seconds(0);
    const auto multiplier=std::isfinite(config.cooldown_multiplier)?std::max(1.0,config.cooldown_multiplier):1.0;
    const auto seconds=static_cast<long double>(initial)*std::pow(static_cast<long double>(multiplier),failures>0?failures-1:0);
    if(seconds>=static_cast<long double>(maximum))return std::chrono::seconds(maximum);
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

std::optional<peer::Endpoint> PeerHealth::endpoint() const {
    crawler::PeerCandidate candidate;candidate.host=host;candidate.port=port;
    candidate.network=network;candidate.address_bytes=address_bytes;
    return candidate.endpoint();
}
std::optional<core::PeerId> PeerHealth::id() const {
    if(auto value=endpoint())return value->id();return std::nullopt;
}
std::string PeerHealth::key() const {
    if(auto value=endpoint())return value->legacy_key();
    return host+":"+std::to_string(port);
}

bool PeerHealth::fresh(std::chrono::system_clock::time_point now, std::chrono::seconds max_age) const noexcept {
    if (last_success.time_since_epoch().count() == 0 || now < last_success) return false;
    return now - last_success <= max_age;
}

double PeerHealth::availability() const noexcept {
    const auto total = static_cast<long double>(successful_probes) + failed_probes;
    return total == 0 ? 0.0 : static_cast<double>(successful_probes) / static_cast<double>(total);
}

PeerHealthCache::PeerHealthCache(HealthConfig config) : config_(std::move(config)) {
    if (config_.minimum_observations_for_hot == 0) config_.minimum_observations_for_hot = 1;
    if (config_.latency_history_size == 0) config_.latency_history_size = 1;
}

PeerHealth* PeerHealthCache::get_or_create(const crawler::PeerCandidate& candidate,
    std::chrono::system_clock::time_point now, bool local_probe) {
    const auto endpoint=candidate.endpoint();
    if(!endpoint || config_.max_peers==0)return nullptr;
    const auto id=endpoint->id();
    auto [it,inserted]=peers_.try_emplace(id);
    auto& peer=it->second;
    if(inserted) {peer.state=PeerState::Candidate;peer.first_seen=now;peer.source=candidate.source;}
    else if(local_probe)peer.source=candidate.source;
    peer.host=endpoint->host();peer.port=endpoint->port().value();
    peer.network=static_cast<crawler::CandidateNetwork>(endpoint->network());
    peer.address_bytes.assign(endpoint->address().begin(),endpoint->address().end());
    peer.transport=transport_for_network(peer.network,peer.host);
    peer.last_seen=now;
    if(local_probe)locally_observed_.insert(id);
    return &peer;
}

void PeerHealthCache::observe_discovered(const crawler::PeerCandidate& candidate) {
    auto* peer=get_or_create(candidate,std::chrono::system_clock::now(),false);
    if(!peer)return;
    increment(peer->observations);
    // Rediscovery is not a probe result: preserve state, failures and quarantine.
    trim();
}

void PeerHealthCache::observe_success(const crawler::PeerCandidate& candidate,
                                      const protocol::ProbeResult& result) {
    if(result.status!=protocol::ProbeStatus::Success)return;
    const auto now = std::chrono::system_clock::now();
    auto* found = get_or_create(candidate, now, true);
    if(!found)return;
    auto& peer=*found;
    increment(peer.observations);
    increment(peer.successful_probes);
    increment(peer.transport_attempts);
    increment(peer.transport_successes);
    increment(peer.consecutive_successes);
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
    if(result.status==protocol::ProbeStatus::Success)return;
    const auto now = std::chrono::system_clock::now();
    auto* found = get_or_create(candidate, now, true);
    if(!found)return;
    auto& peer=*found;
    increment(peer.observations);
    increment(peer.failed_probes);
    increment(peer.transport_attempts);
    increment(peer.transport_failures);
    increment(peer.consecutive_failures);
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
                                     static_cast<long double>(0), [](long double acc, auto v) {
                                         return acc + v.count();
                                     });
    const auto average=sum / peer.latency_history.size();
    peer.latency_avg = std::chrono::milliseconds(average>=static_cast<long double>(std::numeric_limits<std::int64_t>::max())
        ?std::numeric_limits<std::int64_t>::max():static_cast<std::int64_t>(average));
    auto sorted = peer.latency_history;
    std::sort(sorted.begin(), sorted.end());
    const auto index = static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(sorted.size()))) - 1;
    peer.latency_p95 = sorted[std::min(index, sorted.size() - 1)];
}

bool PeerHealthCache::contains(const std::string& key) const {return find(key)!=nullptr;}
const PeerHealth* PeerHealthCache::find(const core::PeerId& id) const noexcept {
    const auto it=peers_.find(id);return it==peers_.end()?nullptr:&it->second;
}
const PeerHealth* PeerHealthCache::find(const std::string& key) const noexcept {
    try {if(auto value=peer::endpoint_from_key(key))return find(value->id());}
    catch(const std::exception&) {} // Preserve the noexcept legacy lookup API.
    return nullptr;
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

std::vector<PeerHealth> PeerHealthCache::select_snapshot(const SelectionPolicy& policy,
    std::chrono::system_clock::time_point now) const {
    if(policy.max_results==0)return {};
    std::vector<const PeerHealth*> candidates;
    for(const auto& [id,peer]:peers_) {
        const bool network=(peer.network==crawler::CandidateNetwork::IPv4 && policy.ipv4) ||
            (peer.network==crawler::CandidateNetwork::IPv6 && policy.ipv6) ||
            (peer.network==crawler::CandidateNetwork::TorV3 && policy.tor_v3) ||
            (peer.network==crawler::CandidateNetwork::I2P && policy.i2p);
        if(network && (peer.state==PeerState::Hot || (policy.include_warm && peer.state==PeerState::Warm)) &&
           peer.fresh(now,policy.freshness) && peer.quarantine_until<=now &&
           (peer.services&policy.required_services)==policy.required_services)candidates.push_back(&peer);
    }
    std::sort(candidates.begin(),candidates.end(),[](const auto* a,const auto* b) {
        if(a->state!=b->state)return a->state==PeerState::Hot;
        if(a->last_success!=b->last_success)return a->last_success>b->last_success;
        return std::tie(a->host,a->port)<std::tie(b->host,b->port);
    });
    std::map<std::vector<std::uint8_t>,std::size_t> groups;
    std::vector<PeerHealth> out;out.reserve(std::min(policy.max_results,candidates.size()));
    for(const auto* peer:candidates) {
        if(policy.max_per_ip_group && (peer->network==crawler::CandidateNetwork::IPv4 || peer->network==crawler::CandidateNetwork::IPv6)) {
            auto endpoint=peer->endpoint();const auto bytes=endpoint->address();
            std::vector<std::uint8_t> group{static_cast<std::uint8_t>(endpoint->network())};
            group.insert(group.end(),bytes.begin(),bytes.begin()+(endpoint->network()==peer::Network::IPv4?2:4));
            if(groups[group]>=policy.max_per_ip_group)continue;
            ++groups[group];
        }
        out.push_back(*peer);if(out.size()==policy.max_results)break;
    }
    return out;
}
std::vector<PeerHealth> PeerHealthCache::dns_snapshot(std::size_t maximum,bool include_warm,
    std::chrono::system_clock::time_point now) const {
    SelectionPolicy policy;policy.max_results=maximum;policy.include_warm=include_warm;policy.freshness=config_.freshness;
    return select_snapshot(policy,now);
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
        add_saturated(s.observations,peer.observations);
        add_saturated(s.successful_probes,peer.successful_probes);
        add_saturated(s.failed_probes,peer.failed_probes);
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
    std::size_t count4=0,count6=0,count_warm=0,count_cold=0;
    for(const auto& [id,p]:peers_) {
        if(p.state==PeerState::Hot && p.network==crawler::CandidateNetwork::IPv4)++count4;
        if(p.state==PeerState::Hot && p.network==crawler::CandidateNetwork::IPv6)++count6;
        if(p.state==PeerState::Warm)++count_warm;
        if(p.state==PeerState::Cold)++count_cold;
    }
    if(peers_.size()<=config_.max_peers && count4<=config_.hot_ipv4 && count6<=config_.hot_ipv6 && count_warm<=config_.warm_max && count_cold<=config_.cold_max)return;
    // Bulk selection gives O(n log n) trimming, not repeated full-cache scans.
    std::vector<decltype(peers_)::iterator> ordered;ordered.reserve(peers_.size());
    for(auto it=peers_.begin();it!=peers_.end();++it)ordered.push_back(it);
    std::sort(ordered.begin(),ordered.end(),[](auto a,auto b) {
        const auto& x=a->second;const auto& y=b->second;
        if(retention_rank(x.state)!=retention_rank(y.state))return retention_rank(x.state)>retention_rank(y.state);
        if(x.last_success!=y.last_success)return x.last_success>y.last_success;
        if(x.last_seen!=y.last_seen)return x.last_seen>y.last_seen;
        return a->first<b->first;
    });
    std::size_t kept=0,hot4=0,hot6=0,warm=0,cold=0;
    for(auto it:ordered) {
        const auto& p=it->second;bool keep=kept<config_.max_peers;
        if(p.state==PeerState::Hot && p.network==crawler::CandidateNetwork::IPv4)keep=keep && hot4<config_.hot_ipv4;
        if(p.state==PeerState::Hot && p.network==crawler::CandidateNetwork::IPv6)keep=keep && hot6<config_.hot_ipv6;
        if(p.state==PeerState::Warm)keep=keep && warm<config_.warm_max;
        if(p.state==PeerState::Cold)keep=keep && cold<config_.cold_max;
        if(!keep) {locally_observed_.erase(it->first);peers_.erase(it);continue;}
        ++kept;
        if(p.state==PeerState::Hot && p.network==crawler::CandidateNetwork::IPv4)++hot4;
        if(p.state==PeerState::Hot && p.network==crawler::CandidateNetwork::IPv6)++hot6;
        if(p.state==PeerState::Warm)++warm;
        if(p.state==PeerState::Cold)++cold;
    }
}

void PeerHealthCache::restore(const PeerHealth& input) {
    const auto endpoint=input.endpoint();if(!endpoint || config_.max_peers==0)return;
    const auto id=endpoint->id();if(locally_observed_.contains(id))return;
    auto rank=[](const PeerHealth& p) {
        return std::tuple{std::max(p.last_success,p.last_failure),p.last_failure>=p.last_success,
            p.quarantine_until,p.next_probe,
            p.failed_probes,p.consecutive_failures,std::numeric_limits<std::uint64_t>::max()-p.successful_probes,
            static_cast<unsigned>(p.state),p.last_seen,p.services,p.user_agent};
    };
    if(auto it=peers_.find(id);it!=peers_.end() && rank(it->second)>=rank(input))return;
    auto peer=input;peer.host=endpoint->host();peer.port=endpoint->port().value();
    peer.network=static_cast<crawler::CandidateNetwork>(endpoint->network());
    peer.address_bytes.assign(endpoint->address().begin(),endpoint->address().end());
    peer.transport=transport_for_network(peer.network,peer.host);
    if(peer.latency_history.size()>config_.latency_history_size)peer.latency_history.erase(peer.latency_history.begin(),peer.latency_history.end()-config_.latency_history_size);
    for(auto& latency:peer.latency_history)if(latency.count()<0)latency={};
    if(!peer.latency_history.empty())recalculate(peer);
    peers_[id]=std::move(peer);trim();
}

void PeerHealthCache::restore(const std::vector<PeerHealth>& peers) {
    for (const auto& peer : peers) restore(peer);
}

void PeerHealthCache::clear() { peers_.clear(); locally_observed_.clear(); }

} // namespace fastseed::health
