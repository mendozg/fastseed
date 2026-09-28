#include "fastseed/intelligence/network_intelligence.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace fastseed::intelligence {

static double clamp01(double v) noexcept { return std::clamp(v, 0.0, 1.0); }

double NetworkIntelligence::reliability(const health::PeerHealth& p) noexcept {
    const auto n = p.successful_probes + p.failed_probes;
    if (n == 0) return 0.5;
    return clamp01(static_cast<double>(p.successful_probes) / static_cast<double>(n));
}

double NetworkIntelligence::latency(const health::PeerHealth& p) noexcept {
    if (p.latency_avg.count() <= 0) return 0.5;
    constexpr double target = 250.0;
    constexpr double ceiling = 3000.0;
    const double ms = static_cast<double>(p.latency_avg.count());
    if (ms <= target) return 1.0;
    return clamp01(1.0 - (ms - target) / (ceiling - target));
}

double NetworkIntelligence::freshness(const health::PeerHealth& p,
                                       std::chrono::system_clock::time_point now) const noexcept {
    if (p.last_success == std::chrono::system_clock::time_point{}) return 0.0;
    if (now <= p.last_success) return 1.0;
    const double age = std::chrono::duration<double>(now - p.last_success).count();
    if (config_.learning_half_life.count() <= 0) return 0.0;
    return clamp01(std::exp2(-age / static_cast<double>(config_.learning_half_life.count())));
}

double NetworkIntelligence::diversity(const health::PeerHealth& p,
                                       const std::vector<health::PeerHealth>& population) noexcept {
    std::size_t distinct = 0;
    std::unordered_set<std::string> groups;
    for (const auto& x : population) {
        std::string group;
        if (x.network == crawler::CandidateNetwork::IPv4 && x.address_bytes.size() == 4)
            group = "4:" + std::to_string(x.address_bytes[0]) + "." + std::to_string(x.address_bytes[1]) + "." + std::to_string(x.address_bytes[2]);
        else if (x.network == crawler::CandidateNetwork::IPv6 && x.address_bytes.size() == 16) {
            group = "6:";
            for (int i=0;i<6;i++) group += std::to_string(x.address_bytes[i]) + ".";
        } else group = "n:" + std::to_string(static_cast<int>(x.network));
        groups.insert(group);
    }
    const auto before = groups.size();
    std::string self;
    if (p.network == crawler::CandidateNetwork::IPv4 && p.address_bytes.size()==4)
        self = "4:" + std::to_string(p.address_bytes[0]) + "." + std::to_string(p.address_bytes[1]) + "." + std::to_string(p.address_bytes[2]);
    else if (p.network == crawler::CandidateNetwork::IPv6 && p.address_bytes.size()==16) {
        self="6:"; for(int i=0;i<6;i++) self += std::to_string(p.address_bytes[i]) + ".";
    } else self="n:"+std::to_string(static_cast<int>(p.network));
    if (groups.empty()) return 1.0;
    const bool unique = std::count_if(population.begin(), population.end(), [&](const auto& x){
        if (x.network != p.network) return false;
        if (p.network == crawler::CandidateNetwork::IPv4 && x.address_bytes.size()==4 && p.address_bytes.size()==4)
            return x.address_bytes[0]==p.address_bytes[0] && x.address_bytes[1]==p.address_bytes[1] && x.address_bytes[2]==p.address_bytes[2];
        if (p.network == crawler::CandidateNetwork::IPv6 && x.address_bytes.size()==16 && p.address_bytes.size()==16)
            for(int i=0;i<6;i++) if(x.address_bytes[i]!=p.address_bytes[i]) return false;
        return true;
    }) == 1;
    distinct = before;
    const double base = static_cast<double>(distinct) / static_cast<double>(std::max<std::size_t>(1, population.size()));
    return clamp01(unique ? std::min(1.0, base + 0.25) : base);
}

Score NetworkIntelligence::score(const health::PeerHealth& p,
                                 const std::vector<health::PeerHealth>& pop,
                                 std::chrono::system_clock::time_point now) const {
    Score s{reliability(p), latency(p), freshness(p, now), diversity(p, pop), 0.0};
    s.total = clamp01(s.reliability*config_.reliability_weight + s.latency*config_.latency_weight +
                      s.freshness*config_.freshness_weight + s.diversity*config_.diversity_weight);
    return s;
}
}
