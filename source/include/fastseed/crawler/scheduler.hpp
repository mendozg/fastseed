#pragma once

#include "fastseed/health/peer_health.hpp"
#include "fastseed/crawler/candidate.hpp"
#include "fastseed/transport/transport.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace fastseed::crawler {
class AdaptiveCapacityController;
}

namespace fastseed::crawler {

struct SchedulerConfig {
    std::size_t max_batch{256};
    std::chrono::seconds stale_refresh{300};
    std::chrono::seconds cold_refresh{900};
    std::chrono::seconds warm_refresh{300};
    std::chrono::seconds hot_refresh{300};
};

struct NetworkQuota {
    std::size_t max_inflight{256};
    std::size_t max_batch{256};
};

struct ActiveProbeCounts {
    std::size_t direct_tcp{};
    std::size_t tor_socks5{};
    std::size_t i2p_sam{};
};

struct AdaptiveSchedulerConfig : SchedulerConfig {
    NetworkQuota direct_tcp{192, 192};
    NetworkQuota tor_socks5{32, 32};
    NetworkQuota i2p_sam{32, 32};
    std::chrono::seconds minimum_refresh{60};
    std::chrono::seconds maximum_refresh{3600};
    double high_availability{0.90};
    double low_availability{0.50};
    std::chrono::milliseconds high_latency{250};
    std::chrono::milliseconds low_latency{1500};
    double exploration_fraction{0.20};
    std::size_t minimum_exploration{1};
    std::size_t maximum_exploration{32};
    double low_confidence_threshold{0.50};
    std::chrono::seconds exploration_rotation{60};
};

struct ScheduledPeer {
    PeerCandidate candidate;
    std::chrono::system_clock::time_point due{};
    int priority{};
};

struct ExplorationTransportTelemetry {
    std::uint64_t budget_allocated{};
    std::uint64_t opportunities{};
    std::uint64_t probes_selected{};
    std::uint64_t quota_rejections{};
    std::uint64_t starvation_windows{};
};

struct ExplorationTelemetrySnapshot {
    std::uint64_t scheduling_cycles{};
    std::uint64_t exploration_budget{};
    std::uint64_t exploration_used{};
    std::array<ExplorationTransportTelemetry, 3> transport{};
};

class ExplorationTelemetry {
public:
    void begin_cycle(std::size_t budget) noexcept;
    void allocate(transport::TransportKind kind) noexcept;
    void opportunity(transport::TransportKind kind) noexcept;
    void selected(transport::TransportKind kind) noexcept;
    void quota_rejection(transport::TransportKind kind) noexcept;
    void starvation(transport::TransportKind kind) noexcept;
    void exploration_used() noexcept;
    ExplorationTelemetrySnapshot snapshot() const noexcept;

private:
    static constexpr std::size_t index(transport::TransportKind kind) noexcept {
        switch (kind) {
        case transport::TransportKind::DirectTcp: return 0;
        case transport::TransportKind::TorSocks5: return 1;
        case transport::TransportKind::I2pSam: return 2;
        }
        return 0;
    }
    std::atomic<std::uint64_t> cycles_{0};
    std::atomic<std::uint64_t> budget_{0};
    std::atomic<std::uint64_t> used_{0};
    std::array<std::atomic<std::uint64_t>, 3> allocated_{};
    std::array<std::atomic<std::uint64_t>, 3> opportunities_{};
    std::array<std::atomic<std::uint64_t>, 3> selected_{};
    std::array<std::atomic<std::uint64_t>, 3> quota_rejections_{};
    std::array<std::atomic<std::uint64_t>, 3> starvation_{};
};

class HealthAwareScheduler {
public:
    explicit HealthAwareScheduler(SchedulerConfig config = {}) : config_(config) {}

    std::vector<ScheduledPeer> select_due(
        const std::vector<health::PeerHealth>& peers,
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;

    static PeerCandidate candidate_from_health(const health::PeerHealth& peer);
    static int priority(const health::PeerHealth& peer,
                        std::chrono::system_clock::time_point now);

private:
    SchedulerConfig config_;
};

class AdaptiveProbeScheduler {
public:
    explicit AdaptiveProbeScheduler(AdaptiveSchedulerConfig config = {}) : config_(config) {}

    std::chrono::seconds recommended_refresh(
        const health::PeerHealth& peer) const noexcept;

    std::chrono::system_clock::time_point effective_due(
        const health::PeerHealth& peer,
        std::chrono::system_clock::time_point now) const noexcept;

    std::vector<ScheduledPeer> select_due(
        const std::vector<health::PeerHealth>& peers,
        ActiveProbeCounts active = {},
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;

    // Confidence-aware selection: low-evidence transports receive a bounded
    // exploration boost, while the adaptive controller remains authoritative
    // for transport capacity and the global ceiling.
    std::vector<ScheduledPeer> select_due(
        const std::vector<health::PeerHealth>& peers,
        const AdaptiveCapacityController& controller,
        ActiveProbeCounts active = {},
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;

    static transport::TransportKind transport_for(const health::PeerHealth& peer) noexcept;
    ExplorationTelemetrySnapshot telemetry() const noexcept { return telemetry_.snapshot(); }

private:
    AdaptiveSchedulerConfig config_;

    NetworkQuota quota_for(transport::TransportKind kind) const noexcept;
    std::size_t active_for(transport::TransportKind kind, const ActiveProbeCounts& active) const noexcept;
    mutable ExplorationTelemetry telemetry_;
};

} // namespace fastseed::crawler
