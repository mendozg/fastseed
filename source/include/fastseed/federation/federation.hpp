#pragma once

#include "fastseed/health/peer_health.hpp"
#include <chrono>
#include <array>
#include <cstdint>
#include <span>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fastseed::federation {

enum class MessageType : std::uint8_t { Hello=1, PeerSync=2, PeerWithdraw=3, Ack=4 };

enum class PeerAction : std::uint8_t { Upsert=1, Withdraw=2 };

struct FederationConfig {
    std::string seeder_id{"bsty-fastseed"};
    std::string shared_secret{};
    std::chrono::seconds clock_skew{120};
    std::chrono::seconds replay_window{300};
    std::size_t max_peers_per_message{256};
    std::size_t max_message_size{65536};
};

struct PeerRecord {
    std::string host;
    std::uint16_t port{};
    std::int32_t protocol_version{};
    std::uint64_t services{};
    std::uint64_t successful_probes{};
    std::uint64_t failed_probes{};
    std::int64_t last_success_unix{};
    std::int64_t last_failure_unix{};
    std::uint64_t observed_at_unix{};
    std::string source_seeder;

    std::string key() const;
};

struct FederationMessage {
    MessageType type{MessageType::PeerSync};
    std::uint8_t version{1};
    std::string sender_id;
    std::uint64_t sequence{};
    std::int64_t timestamp_unix{};
    std::vector<PeerRecord> peers;
    std::vector<std::string> withdrawals;
    std::array<std::uint8_t,32> auth_tag{};
};

std::vector<std::uint8_t> encode(const FederationMessage& message,
                                 std::string_view shared_secret,
                                 std::size_t max_message_size = 65536);
FederationMessage decode(std::span<const std::uint8_t> wire,
                         std::string_view shared_secret,
                         const FederationConfig& config = {});

class ReplayGuard {
public:
    explicit ReplayGuard(std::size_t max_entries = 10000) : max_entries_(max_entries) {}
    bool accept(std::string_view sender, std::uint64_t sequence,
                std::int64_t timestamp, std::int64_t now_unix,
                std::chrono::seconds window);
    void clear();
private:
    struct Entry { std::uint64_t sequence{}; std::int64_t timestamp{}; };
    std::size_t max_entries_;
    std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

class FederatedPeerStore {
public:
    explicit FederatedPeerStore(std::string local_seeder = {});

    // Apply a federation message. Returns the number of peer records whose
    // effective state changed. Withdrawals remove only known records.
    std::size_t apply(const FederationMessage& message);

    // Export a bounded snapshot. Records learned from the requesting seeder
    // are excluded to reduce rebroadcast loops.
    std::vector<PeerRecord> snapshot(std::size_t limit = 256,
                                     std::string_view exclude_source = {}) const;

    bool contains(std::string_view key) const;
    std::size_t size() const noexcept { return peers_.size(); }
    void clear() noexcept { peers_.clear(); }

private:
    std::string local_seeder_;
    std::unordered_map<std::string, PeerRecord> peers_;
};

class SeederMesh {
public:
    explicit SeederMesh(FederationConfig config = {});
    std::string seeder_id() const { return config_.seeder_id; }
    std::vector<std::uint8_t> make_hello(std::uint64_t sequence, std::int64_t now_unix) const;
    std::vector<std::uint8_t> make_sync(std::uint64_t sequence,
                                        std::int64_t now_unix,
                                        std::vector<PeerRecord> peers) const;
    std::vector<std::uint8_t> make_withdraw(std::uint64_t sequence,
                                            std::int64_t now_unix,
                                            std::vector<std::string> keys) const;
    bool accept(std::span<const std::uint8_t> wire, std::int64_t now_unix,
                FederationMessage& out);
private:
    FederationConfig config_;
    ReplayGuard replay_;
};

} // namespace fastseed::federation
