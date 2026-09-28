#pragma once
#include "fastseed/health/peer_health.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace fastseed::intelligence {

struct Config {
    std::chrono::seconds learning_half_life{21600};
    std::size_t minimum_observations{3};
    double reliability_weight{0.40};
    double latency_weight{0.25};
    double freshness_weight{0.15};
    double diversity_weight{0.20};
};

struct Score {
    double reliability{};
    double latency{};
    double freshness{};
    double diversity{};
    double total{};
};

class NetworkIntelligence {
public:
    explicit NetworkIntelligence(Config config = {}) : config_(config) {}

    Score score(const health::PeerHealth& peer,
                const std::vector<health::PeerHealth>& population,
                std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;

    static double reliability(const health::PeerHealth&) noexcept;
    static double latency(const health::PeerHealth&) noexcept;
    double freshness(const health::PeerHealth&, std::chrono::system_clock::time_point) const noexcept;
    static double diversity(const health::PeerHealth&, const std::vector<health::PeerHealth>&) noexcept;

private:
    Config config_;
};

} // namespace fastseed::intelligence
