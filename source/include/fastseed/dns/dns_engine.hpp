#pragma once

#include "fastseed/health/peer_health.hpp"
#include "fastseed/dns/rate_limiter.hpp"
#include <boost/asio.hpp>
#include <cstdint>
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fastseed::dns {

constexpr std::uint16_t TYPE_A = 1;
constexpr std::uint16_t TYPE_AAAA = 28;
constexpr std::uint16_t CLASS_IN = 1;

enum class QueryResult { Answered, NoData, Refused, Malformed };

struct DnsConfig {
    std::string zone{"seed.globalboost.info"};
    std::uint16_t listen_port{53};
    std::uint32_t ttl_seconds{60};
    std::size_t max_answers{32};
    bool include_warm{true};
    std::size_t max_response_size{1232};
    RateLimiter::Config rate_limit{};
};

struct DnsSnapshot {
    std::vector<health::PeerHealth> peers;
    std::uint64_t generation{0};
};

class SnapshotStore {
public:
    void publish(std::shared_ptr<const DnsSnapshot> snapshot) noexcept;
    std::shared_ptr<const DnsSnapshot> load() const noexcept;
private:
    std::atomic<std::shared_ptr<const DnsSnapshot>> snapshot_;
};

struct DnsQuestion {
    std::string name;
    std::uint16_t type{};
    std::uint16_t klass{};
};

struct DnsAnswer {
    std::string host;
    std::uint16_t type{};
    std::vector<std::uint8_t> address;
    std::uint32_t ttl{};
};

class DnsResponseEngine {
public:
    explicit DnsResponseEngine(DnsConfig config = {});

    void publish_from_cache(const health::PeerHealthCache& cache,
                            std::uint64_t generation = 0);
    std::shared_ptr<const DnsSnapshot> snapshot() const noexcept;

    std::vector<std::uint8_t> build_response(std::span<const std::uint8_t> query) const;
    QueryResult parse_question(std::span<const std::uint8_t> query,
                               DnsQuestion& question) const;
    std::vector<DnsAnswer> select_answers(const DnsQuestion& question,
                                          const DnsSnapshot& snapshot) const;

private:
    DnsConfig config_;
    SnapshotStore store_;

    static std::string normalize_name(std::string_view name);
    bool name_matches_zone(std::string_view name) const;
};

class UdpServer {
public:
    UdpServer(boost::asio::io_context& io,
              DnsResponseEngine& engine,
              std::uint16_t port = 53,
              const boost::asio::ip::address& bind_address = boost::asio::ip::address_v4::any(),
              RateLimiter::Config rate_limit = RateLimiter::Config{});
    void start();
    void stop();
    bool running() const noexcept { return socket_.is_open(); }
private:
    void receive();
    void send(std::size_t length);
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint remote_;
    std::array<std::uint8_t, 4096> buffer_{};
    DnsResponseEngine& engine_;
    std::vector<std::uint8_t> response_;
    RateLimiter limiter_;
};

} // namespace fastseed::dns
