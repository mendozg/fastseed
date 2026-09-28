#pragma once
#include "candidate.hpp"
#include "scheduler.hpp"
#include "fastseed/protocol/probe.hpp"
#include "fastseed/protocol/wire_messages.hpp"
#include "fastseed/transport/transport.hpp"
#include <boost/asio/io_context.hpp>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "fastseed/crawler/adaptive_feedback.hpp"

namespace fastseed::crawler {

struct CrawlerConfig {
    std::size_t max_concurrent_probes{256};
    std::size_t max_candidates{100000};
    std::size_t max_open_connections{2048};
    std::chrono::milliseconds connect_timeout{1500};
    std::chrono::milliseconds handshake_timeout{2500};
    std::chrono::milliseconds addr_exchange_timeout{750};
    std::chrono::seconds cooldown_initial{30};
    std::chrono::seconds cooldown_max{3600};
    double cooldown_multiplier{2.0};
    bool enable_tor_transport{false};
    bool enable_i2p_transport{false};
    transport::TorSocks5Transport::Config tor_transport{};
    transport::I2pSamTransport::Config i2p_transport{};
    transport::I2pSamTransport::DestinationResolver i2p_destination_resolver{};
};

struct CrawlerStats {
    std::uint64_t candidates_total{};
    std::uint64_t queued{};
    std::uint64_t active_probes{};
    std::uint64_t validated{};
    std::uint64_t failed{};
    std::uint64_t timeouts{};
    std::uint64_t handshake_failures{};
    std::uint64_t connection_failures{};
    std::uint64_t unsupported_versions{};
    std::uint64_t ipv4_candidates{};
    std::uint64_t ipv6_candidates{};
    std::uint64_t torv3_candidates{};
    std::uint64_t i2p_candidates{};
    std::uint64_t successful_probes{};
    std::uint64_t addrv2_messages{};
    std::uint64_t addrv2_addresses{};
    std::uint64_t transport_attempts{};
    std::uint64_t transport_failures{};
    std::uint64_t transport_unsupported{};
};

class ProbeRunner {
public:
    virtual ~ProbeRunner() = default;
    virtual void run(const std::string& host, std::uint16_t port,
                     std::chrono::milliseconds connect_timeout,
                     std::chrono::milliseconds handshake_timeout,
                     std::function<void(protocol::ProbeResult)> done) = 0;
    virtual void run_candidate(const PeerCandidate& candidate,
                               std::chrono::milliseconds connect_timeout,
                               std::chrono::milliseconds handshake_timeout,
                               std::function<void(protocol::ProbeResult)> done) {
        run(candidate.host, candidate.port, connect_timeout, handshake_timeout, std::move(done));
    }
};

class AsioProbeRunner final : public ProbeRunner {
public:
    explicit AsioProbeRunner(boost::asio::io_context& io) : probe_(io) {}
    void run(const std::string& host, std::uint16_t port,
             std::chrono::milliseconds connect_timeout,
             std::chrono::milliseconds handshake_timeout,
             std::function<void(protocol::ProbeResult)> done) override;
private:
    protocol::PeerProbe probe_;
};

class TransportProbeRunner final : public ProbeRunner {
public:
    explicit TransportProbeRunner(boost::asio::io_context& io, const CrawlerConfig& config = {});
    void run(const std::string& host, std::uint16_t port,
             std::chrono::milliseconds connect_timeout,
             std::chrono::milliseconds handshake_timeout,
             std::function<void(protocol::ProbeResult)> done) override;
    void run_candidate(const PeerCandidate& candidate,
                       std::chrono::milliseconds connect_timeout,
                       std::chrono::milliseconds handshake_timeout,
                       std::function<void(protocol::ProbeResult)> done) override;
    transport::TransportRouter& router() noexcept { return router_; }
private:
    boost::asio::io_context& io_;
    transport::TransportRouter router_;
    protocol::PeerProbe probe_;
};

class PeerCrawler {
public:
    using ValidatedCallback = std::function<void(const PeerCandidate&, const protocol::ProbeResult&)>;

    explicit PeerCrawler(boost::asio::io_context& io, CrawlerConfig config = {});
    PeerCrawler(boost::asio::io_context& io, CrawlerConfig config,
                std::unique_ptr<ProbeRunner> runner);

    bool add_candidate(std::string host, std::uint16_t port, CandidateSource source);
    bool add_addrv2_candidate(const protocol::AddrV2Address& address);
    void add_candidates(const std::vector<PeerCandidate>& candidates);
    std::size_t schedule_from_health(const std::vector<health::PeerHealth>& peers,
                                     std::chrono::system_clock::time_point now = std::chrono::system_clock::now());
    void add_bootstrap_seed(std::string host, std::uint16_t port = 0);
    void start();
    void stop();
    bool running() const noexcept { return running_; }
    CrawlerStats stats() const noexcept;
    std::size_t transport_capacity(transport::TransportKind kind) const noexcept;
    std::vector<PeerCandidate> snapshot() const;
    void set_validated_callback(ValidatedCallback cb) { validated_callback_ = std::move(cb); }

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
    bool running_{false};
    ValidatedCallback validated_callback_;
};

} // namespace fastseed::crawler
