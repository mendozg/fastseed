#pragma once

#include "fastseed/crawler/candidate.hpp"
#include "fastseed/protocol/probe.hpp"
#include "fastseed/transport/transport.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace fastseed::health {

enum class PeerState {
    Discovered,
    Candidate,
    Probing,
    Validated,
    Warm,
    Hot,
    Cold,
    Cooldown,
    Quarantined
};

std::string_view to_string(PeerState state) noexcept;

struct HealthConfig {
    std::size_t hot_ipv4{1000};
    std::size_t hot_ipv6{500};
    std::size_t warm_max{10000};
    std::size_t cold_max{50000};
    std::size_t minimum_observations_for_hot{3};
    std::chrono::seconds freshness{1800};
    std::chrono::seconds quarantine{3600};
    std::chrono::seconds cooldown_initial{30};
    std::chrono::seconds cooldown_max{3600};
    double cooldown_multiplier{2.0};
    std::size_t quarantine_after_failures{3};
    std::size_t latency_history_size{32};
};

struct PeerHealth {
    transport::TransportKind transport{transport::TransportKind::DirectTcp};
    std::string host;
    std::uint16_t port{};
    crawler::CandidateSource source{crawler::CandidateSource::Manual};
    PeerState state{PeerState::Discovered};
    crawler::CandidateNetwork network{crawler::CandidateNetwork::Unknown};
    std::vector<std::uint8_t> address_bytes;

    std::int32_t protocol_version{};
    std::uint64_t services{};
    std::string user_agent;

    std::uint64_t observations{};
    std::uint64_t successful_probes{};
    std::uint64_t failed_probes{};
    std::uint32_t consecutive_successes{};
    std::uint32_t consecutive_failures{};
    std::uint64_t transport_attempts{};
    std::uint64_t transport_successes{};
    std::uint64_t transport_failures{};

    std::chrono::system_clock::time_point first_seen{};
    std::chrono::system_clock::time_point last_seen{};
    std::chrono::system_clock::time_point last_success{};
    std::chrono::system_clock::time_point last_failure{};
    std::chrono::system_clock::time_point quarantine_until{};
    std::chrono::system_clock::time_point next_probe{};

    std::chrono::milliseconds latency_last{};
    std::chrono::milliseconds latency_avg{};
    std::chrono::milliseconds latency_p95{};
    std::vector<std::chrono::milliseconds> latency_history;

    std::string last_error;
    std::string transport_last_error;

    std::string key() const;
    bool fresh(std::chrono::system_clock::time_point now,
               std::chrono::seconds max_age = std::chrono::seconds(1800)) const noexcept;
    double availability() const noexcept;
};

struct HealthStats {
    std::size_t total{};
    std::size_t hot_ipv4{};
    std::size_t hot_ipv6{};
    std::size_t warm{};
    std::size_t cold{};
    std::size_t quarantined{};
    std::size_t hot_torv3{};
    std::size_t hot_i2p{};
    std::uint64_t observations{};
    std::uint64_t successful_probes{};
    std::uint64_t failed_probes{};
};

class PeerHealthCache {
public:
    explicit PeerHealthCache(HealthConfig config = {});

    void observe_discovered(const crawler::PeerCandidate& candidate);
    void observe_success(const crawler::PeerCandidate& candidate,
                         const protocol::ProbeResult& result);
    void observe_failure(const crawler::PeerCandidate& candidate,
                         const protocol::ProbeResult& result);
    void restore(const PeerHealth& peer);
    void restore(const std::vector<PeerHealth>& peers);

    bool contains(const std::string& key) const;
    const PeerHealth* find(const std::string& key) const noexcept;

    std::vector<PeerHealth> snapshot() const;
    std::vector<PeerHealth> hot_snapshot() const;
    std::vector<PeerHealth> dns_snapshot(std::size_t max_results,
                                         bool include_warm = true,
                                         std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;

    HealthStats stats() const noexcept;
    void prune(std::chrono::system_clock::time_point now = std::chrono::system_clock::now());
    void clear();

private:
    HealthConfig config_;
    std::unordered_map<std::string, PeerHealth> peers_;

    PeerHealth& get_or_create(const crawler::PeerCandidate& candidate,
                              std::chrono::system_clock::time_point now);
    void trim();
    void recalculate(PeerHealth& peer);
};

} // namespace fastseed::health
