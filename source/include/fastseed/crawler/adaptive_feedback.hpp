#pragma once

#include "fastseed/crawler/scheduler.hpp"
#include <cstddef>
#include <cstdint>
#include <chrono>

namespace fastseed::crawler {

enum class ProbeOutcome { Success, Failure, Timeout, HandshakeFailure, Unsupported };

struct TransportCapacity {
    std::size_t minimum{1};
    std::size_t target{32};
    std::size_t maximum{256};
};

struct FeedbackConfig {
    std::size_t global_max_inflight{256};
    TransportCapacity direct_tcp{8, 192, 192};
    TransportCapacity tor_socks5{2, 16, 32};
    TransportCapacity i2p_sam{2, 16, 32};
    double increase_success_rate{0.90};
    double decrease_success_rate{0.50};
    double increase_latency_ms{250.0};
    double decrease_latency_ms{1500.0};
    std::size_t adjustment_step{8};
    std::size_t failure_streak_for_decrease{3};
    std::chrono::seconds cooldown_after_adjustment{30};
    std::chrono::hours learning_half_life{6};
    std::chrono::hours capacity_recovery_period{24};
};

struct PersistedTransportState {
    std::uint32_t state_version{1};
    std::uint64_t attempts{};
    std::uint64_t successes{};
    std::uint64_t failures{};
    std::uint64_t timeouts{};
    std::uint64_t handshake_failures{};
    std::uint64_t unsupported{};
    std::uint32_t consecutive_failures{};
    std::uint32_t consecutive_successes{};
    std::int64_t latency_avg_ms{};
    std::size_t learned_capacity{};
    std::int64_t updated_at_ms{};
};

struct TransportFeedbackStats {
    std::uint64_t attempts{};
    std::uint64_t successes{};
    std::uint64_t failures{};
    std::uint64_t timeouts{};
    std::uint64_t handshake_failures{};
    std::uint64_t unsupported{};
    std::uint32_t consecutive_failures{};
    std::uint32_t consecutive_successes{};
    std::chrono::milliseconds latency_avg{};
};

class AdaptiveCapacityController {
public:
    explicit AdaptiveCapacityController(FeedbackConfig config = {});

    void record(transport::TransportKind kind, ProbeOutcome outcome,
                std::chrono::milliseconds latency = {});
    std::size_t capacity(transport::TransportKind kind) const noexcept;
    TransportFeedbackStats stats(transport::TransportKind kind) const noexcept;
    bool admit(transport::TransportKind kind, std::size_t active) const noexcept;
    std::size_t total_capacity() const noexcept;

    void reset(transport::TransportKind kind) noexcept;
    PersistedTransportState snapshot(transport::TransportKind kind) const noexcept;
    // Decays historical evidence toward a neutral baseline. Returns true when state changed.
    bool decay(transport::TransportKind kind,
               std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) noexcept;
    bool restore(transport::TransportKind kind, const PersistedTransportState& persisted,
                 std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) noexcept;

    // Evidence strength, independent of whether the evidence is positive or negative.
    double confidence(transport::TransportKind kind,
                      std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const noexcept;
    double effective_observations(transport::TransportKind kind,
                      std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const noexcept;

private:
    FeedbackConfig config_;
    struct State { TransportFeedbackStats stats; std::size_t capacity{}; std::chrono::system_clock::time_point last_adjustment{}; } direct_, tor_, i2p_;
    State& state(transport::TransportKind kind) noexcept;
    const State& state(transport::TransportKind kind) const noexcept;
    TransportCapacity limits(transport::TransportKind kind) const noexcept;
    void maybe_adjust(transport::TransportKind kind, State& state);
};

} // namespace fastseed::crawler
