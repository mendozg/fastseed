#include "fastseed/crawler/crawler.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/protocol_limits.hpp"
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>
#include <utility>

namespace fastseed::crawler {

void AsioProbeRunner::run(const std::string& host, std::uint16_t port,
                          std::chrono::milliseconds ct,
                          std::chrono::milliseconds ht,
                          std::function<void(protocol::ProbeResult)> done) {
    probe_.run(host, port, ct, ht, std::move(done));
}

TransportProbeRunner::TransportProbeRunner(boost::asio::io_context& io, const CrawlerConfig& config)
    : io_(io), router_(io), probe_(io) {
    router_.set_direct(std::make_shared<transport::DirectTcpTransport>(io));
    if (config.enable_tor_transport)
        router_.set_tor(std::make_shared<transport::TorSocks5Transport>(io, config.tor_transport));
    if (config.enable_i2p_transport)
        router_.set_i2p(std::make_shared<transport::I2pSamTransport>(io, config.i2p_transport, config.i2p_destination_resolver));
}

void TransportProbeRunner::run(const std::string& host, std::uint16_t port,
                               std::chrono::milliseconds ct,
                               std::chrono::milliseconds ht,
                               std::function<void(protocol::ProbeResult)> done) {
    PeerCandidate c;
    c.host = host; c.port = port;
    c.network = (host.find(':') != std::string::npos) ? CandidateNetwork::IPv6 : CandidateNetwork::IPv4;
    run_candidate(c, ct, ht, std::move(done));
}

void TransportProbeRunner::run_candidate(const PeerCandidate& candidate,
                                          std::chrono::milliseconds ct,
                                          std::chrono::milliseconds ht,
                                          std::function<void(protocol::ProbeResult)> done) {
    router_.connect(candidate, ct, [this, candidate, ht, done = std::move(done)](transport::TransportResult tr) mutable {
        if (tr.status != transport::TransportStatus::Connected || !tr.socket) {
            protocol::ProbeResult r;
            r.endpoint = tr.endpoint;
            switch (tr.status) {
                case transport::TransportStatus::Unsupported:
                    r.status = protocol::ProbeStatus::TransportUnsupported; break;
                case transport::TransportStatus::Timeout:
                    r.status = protocol::ProbeStatus::Timeout; break;
                case transport::TransportStatus::ConnectionFailed:
                    r.status = protocol::ProbeStatus::ConnectionFailed; break;
                case transport::TransportStatus::InvalidTarget:
                    r.status = protocol::ProbeStatus::InvalidMessage; break;
                case transport::TransportStatus::HandshakeFailed:
                    r.status = protocol::ProbeStatus::HandshakeFailed; break;
                case transport::TransportStatus::Connected:
                    r.status = protocol::ProbeStatus::ConnectionFailed; break;
            }
            r.error = tr.error.empty() ? transport::transport_name(tr.kind) : tr.error;
            done(std::move(r));
            return;
        }
        probe_.run_connected(std::move(tr.socket), tr.endpoint, ht, std::move(done));
    });
}

struct PeerCrawler::Impl : std::enable_shared_from_this<PeerCrawler::Impl> {
    boost::asio::io_context& io;
    boost::asio::ip::tcp::resolver resolver;
    CrawlerConfig cfg;
    std::unique_ptr<ProbeRunner> runner;
    std::deque<std::string> queue;
    std::unordered_map<std::string, PeerCandidate> candidates;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> cooldown_until;
    std::unordered_map<std::string, std::shared_ptr<boost::asio::steady_timer>> cooldown_timers;
    std::size_t active{0};
    bool running{false};
    CrawlerStats stats{};
    ValidatedCallback callback;
    AdaptiveCapacityController capacity_controller;

    Impl(boost::asio::io_context& i, CrawlerConfig c, std::unique_ptr<ProbeRunner> r)
        : io(i), resolver(i), cfg(std::move(c)), runner(std::move(r)), capacity_controller() {}

    static bool is_ipv6(const std::string& host) {
        boost::system::error_code ec;
        auto a = boost::asio::ip::make_address(host, ec);
        return !ec && a.is_v6();
    }

    bool add_addrv2(const protocol::AddrV2Address& address, CandidateSource source = CandidateSource::PeerExchange) {
        if (candidates.size() >= cfg.max_candidates) return false;
        auto c = make_addrv2_candidate(address, source);
        if (c.network == CandidateNetwork::TorV3) ++stats.torv3_candidates;
        if (c.network == CandidateNetwork::I2P) ++stats.i2p_candidates;
        if (c.network == CandidateNetwork::IPv4) ++stats.ipv4_candidates;
        if (c.network == CandidateNetwork::IPv6) ++stats.ipv6_candidates;
        const auto key = c.key();
        auto it = candidates.find(key);
        if (it != candidates.end()) return false;
        candidates.emplace(key, std::move(c));
        ++stats.candidates_total;
        auto& inserted = candidates.at(key);
        if (inserted.state == CandidateState::Queued) {
            ++stats.queued;
            queue.push_back(key);
        }
        if (running) pump();
        return true;
    }

    bool add(std::string host, std::uint16_t port, CandidateSource source) {
        if (host.empty() || host.size() > protocol::limits::max_host_size || port == 0 || candidates.size() >= cfg.max_candidates) return false;
        std::string key = host + ":" + std::to_string(port);
        auto it = candidates.find(key);
        if (it != candidates.end()) {
            if (it->second.state == CandidateState::Failed || it->second.state == CandidateState::Cooldown) {
                it->second.state = CandidateState::Queued;
                it->second.source = source;
                queue.push_back(key);
                ++stats.queued;
                return true;
            }
            return false;
        }
        PeerCandidate c; c.host=std::move(host); c.port=port; c.source=source; c.network=is_ipv6(c.host) ? CandidateNetwork::IPv6 : CandidateNetwork::IPv4; c.state=CandidateState::Queued;
        candidates.emplace(key, c);
        queue.push_back(key);
        ++stats.candidates_total;
        ++stats.queued;
        if (candidates.at(key).network == CandidateNetwork::IPv6) ++stats.ipv6_candidates;
        else ++stats.ipv4_candidates;
        if (running) pump();
        return true;
    }

    void start() { if (!running) { running = true; pump(); } }
    void stop() { running = false; queue.clear(); boost::system::error_code ec; resolver.cancel(); for (auto& [key,timer] : cooldown_timers) timer->cancel(ec); cooldown_timers.clear(); }

    void resolve_bootstrap(std::string host, std::uint16_t port) {
        if (host.empty() || port == 0) return;
        auto self = shared_from_this();
        resolver.async_resolve(host, std::to_string(port),
            [self](boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results) {
                if (ec || !self->running) return;
                for (const auto& entry : results) {
                    self->add(entry.endpoint().address().to_string(), entry.endpoint().port(), CandidateSource::Bootstrap);
                }
            });
    }

    std::size_t active_for(transport::TransportKind kind) const noexcept {
        std::size_t count = 0;
        for (const auto& [key, candidate] : candidates) {
            if (candidate.state != CandidateState::Probing) continue;
            const auto candidate_kind = candidate.network == CandidateNetwork::TorV3
                ? transport::TransportKind::TorSocks5
                : candidate.network == CandidateNetwork::I2P
                    ? transport::TransportKind::I2pSam
                    : transport::TransportKind::DirectTcp;
            if (candidate_kind == kind) ++count;
        }
        return count;
    }

    void record_feedback(const PeerCandidate& candidate, const protocol::ProbeResult& result) {
        const auto kind = candidate.network == CandidateNetwork::TorV3
            ? transport::TransportKind::TorSocks5
            : candidate.network == CandidateNetwork::I2P
                ? transport::TransportKind::I2pSam
                : transport::TransportKind::DirectTcp;
        crawler::ProbeOutcome outcome = crawler::ProbeOutcome::Failure;
        switch (result.status) {
            case protocol::ProbeStatus::Success: outcome = crawler::ProbeOutcome::Success; break;
            case protocol::ProbeStatus::Timeout: outcome = crawler::ProbeOutcome::Timeout; break;
            case protocol::ProbeStatus::HandshakeFailed: outcome = crawler::ProbeOutcome::HandshakeFailure; break;
            case protocol::ProbeStatus::TransportUnsupported: outcome = crawler::ProbeOutcome::Unsupported; break;
            default: outcome = crawler::ProbeOutcome::Failure; break;
        }
        const auto latency = result.connect_time + result.handshake_time;
        capacity_controller.record(kind, outcome, latency);
    }

    void pump() {
        if (!running) return;
        while (active < cfg.max_concurrent_probes && active < cfg.max_open_connections && !queue.empty()) {
            std::string key;
            bool found = false;
            const auto scan = queue.size();
            for (std::size_t i = 0; i < scan; ++i) {
                auto candidate_key = queue.front(); queue.pop_front();
                auto it = candidates.find(candidate_key);
                if (it == candidates.end() || it->second.state != CandidateState::Queued) continue;
                if (it->second.network == CandidateNetwork::Unknown) {
                    it->second.state = CandidateState::AwaitingTransport;
                    continue;
                }
                auto now = std::chrono::steady_clock::now();
                auto cd = cooldown_until.find(candidate_key);
                if (cd != cooldown_until.end() && now < cd->second) {
                    queue.push_back(candidate_key);
                    continue;
                }
                const auto kind = it->second.network == CandidateNetwork::TorV3
                    ? transport::TransportKind::TorSocks5
                    : it->second.network == CandidateNetwork::I2P
                        ? transport::TransportKind::I2pSam
                        : transport::TransportKind::DirectTcp;
                if (active >= capacity_controller.total_capacity() ||
                    !capacity_controller.admit(kind, active_for(kind))) {
                    queue.push_back(candidate_key);
                    continue;
                }
                key = std::move(candidate_key);
                found = true;
                break;
            }
            if (!found) break;
            auto it = candidates.find(key);
            if (it == candidates.end() || it->second.state != CandidateState::Queued) continue;
            cooldown_until.erase(key);
            it->second.state = CandidateState::Probing;
            ++it->second.attempts;
            --stats.queued;
            ++active;
            ++stats.active_probes;
            auto self = shared_from_this();
            const auto candidate = it->second;
            runner->run_candidate(candidate, cfg.connect_timeout, cfg.handshake_timeout,
                [self, key](protocol::ProbeResult result) { self->complete(key, std::move(result)); });
        }
    }

    void complete(const std::string& key, protocol::ProbeResult result) {
        if (active) --active;
        if (stats.active_probes) --stats.active_probes;
        auto it = candidates.find(key);
        if (it == candidates.end()) { pump(); return; }
        auto& c = it->second;
        record_feedback(c, result);
        if (result.status == protocol::ProbeStatus::TransportUnsupported) {
            c.state = CandidateState::AwaitingTransport;
            ++stats.transport_unsupported;
            pump();
            return;
        }
        if (c.network == CandidateNetwork::TorV3 || c.network == CandidateNetwork::I2P) {
            ++stats.transport_attempts;
            if (result.status != protocol::ProbeStatus::Success) ++stats.transport_failures;
        }
        if (result.status == protocol::ProbeStatus::Success) {
            c.state = CandidateState::Validated;
            ++c.successes; ++stats.validated; ++stats.successful_probes;
            if (!result.discovered_addrv2.empty()) {
                ++stats.addrv2_messages;
                stats.addrv2_addresses += result.discovered_addrv2.size();
                for (const auto& address : result.discovered_addrv2) {
                    try {
                        add_addrv2(address, CandidateSource::PeerExchange);
                    } catch (const std::exception&) {
                        // A malformed/unsupported record should already have been
                        // rejected by the protocol decoder. Keep crawler processing
                        // resilient if candidate conversion fails independently.
                    }
                }
            }
            if (callback) callback(c, result);
        } else {
            c.state = CandidateState::Cooldown;
            ++c.failures; c.last_error = result.error;
            ++stats.failed;
            switch (result.status) {
                case protocol::ProbeStatus::Timeout: ++stats.timeouts; break;
                case protocol::ProbeStatus::HandshakeFailed: ++stats.handshake_failures; break;
                case protocol::ProbeStatus::ConnectionFailed: ++stats.connection_failures; break;
                case protocol::ProbeStatus::UnsupportedVersion: ++stats.unsupported_versions; break;
                default: break;
            }
            const auto exp = std::min<std::uint32_t>(c.failures - 1, 16);
            auto secs = static_cast<double>(cfg.cooldown_initial.count()) * std::pow(cfg.cooldown_multiplier, exp);
            secs = std::min(secs, static_cast<double>(cfg.cooldown_max.count()));
            auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(static_cast<long long>(secs));
            cooldown_until[key] = expiry;
            auto timer = std::make_shared<boost::asio::steady_timer>(io);
            timer->expires_at(expiry);
            cooldown_timers[key] = timer;
            auto self = shared_from_this();
            timer->async_wait([self, key, timer](boost::system::error_code ec) {
                if (ec || !self->running) return;
                auto it = self->candidates.find(key);
                if (it == self->candidates.end() || it->second.state != CandidateState::Cooldown) return;
                it->second.state = CandidateState::Queued;
                ++self->stats.queued;
                self->queue.push_back(key);
                self->cooldown_until.erase(key);
                self->cooldown_timers.erase(key);
                self->pump();
            });
        }
        pump();
    }
};

PeerCrawler::PeerCrawler(boost::asio::io_context& io, CrawlerConfig config)
    : PeerCrawler(io, config, std::make_unique<TransportProbeRunner>(io, config)) {}

PeerCrawler::PeerCrawler(boost::asio::io_context& io, CrawlerConfig config,
                         std::unique_ptr<ProbeRunner> runner)
    : impl_(std::make_shared<Impl>(io, std::move(config), std::move(runner))) {
    impl_->callback = [this](const PeerCandidate& c, const protocol::ProbeResult& r) {
        if (validated_callback_) validated_callback_(c, r);
    };
}

bool PeerCrawler::add_candidate(std::string host, std::uint16_t port, CandidateSource source) {
    return impl_->add(std::move(host), port, source);
}
bool PeerCrawler::add_addrv2_candidate(const protocol::AddrV2Address& address) {
    if (impl_->candidates.size() >= impl_->cfg.max_candidates) return false;
    auto c = make_addrv2_candidate(address);
    if (c.network == CandidateNetwork::TorV3) ++impl_->stats.torv3_candidates;
    if (c.network == CandidateNetwork::I2P) ++impl_->stats.i2p_candidates;
    if (c.network == CandidateNetwork::IPv4) ++impl_->stats.ipv4_candidates;
    if (c.network == CandidateNetwork::IPv6) ++impl_->stats.ipv6_candidates;
    const auto key = c.key();
    if (impl_->candidates.find(key) != impl_->candidates.end()) return false;
    impl_->candidates.emplace(key, std::move(c));
    ++impl_->stats.candidates_total;
    if (impl_->candidates.at(key).state == CandidateState::Queued) { ++impl_->stats.queued; impl_->queue.push_back(key); }
    return true;
}
void PeerCrawler::add_candidates(const std::vector<PeerCandidate>& cs) {
    for (const auto& c : cs) {
        if (c.network == CandidateNetwork::IPv4 || c.network == CandidateNetwork::IPv6 || c.network == CandidateNetwork::Unknown) {
            add_candidate(c.host, c.port, c.source);
            continue;
        }
        if (impl_->candidates.size() >= impl_->cfg.max_candidates) break;
        const auto key = c.key();
        if (impl_->candidates.find(key) != impl_->candidates.end()) continue;
        impl_->candidates.emplace(key, c);
        ++impl_->stats.candidates_total;
        if (c.network == CandidateNetwork::TorV3) ++impl_->stats.torv3_candidates;
        if (c.network == CandidateNetwork::I2P) ++impl_->stats.i2p_candidates;
        ++impl_->stats.queued;
        impl_->queue.push_back(key);
    }
    if (impl_->running) impl_->pump();
}
std::size_t PeerCrawler::schedule_from_health(const std::vector<health::PeerHealth>& peers,
                                               std::chrono::system_clock::time_point now) {
    HealthAwareScheduler scheduler;
    const auto due = scheduler.select_due(peers, now);
    std::size_t added = 0;
    for (const auto& item : due) {
        auto c = item.candidate;
        if (impl_->candidates.size() >= impl_->cfg.max_candidates) break;
        const auto key = c.key();
        auto it = impl_->candidates.find(key);
        if (it != impl_->candidates.end()) {
            if (it->second.state == CandidateState::Probing) continue;
            it->second = c;
            if (c.state == CandidateState::Queued) {
                ++impl_->stats.queued;
                impl_->queue.push_back(key);
            }
            ++added;
            continue;
        }
        impl_->candidates.emplace(key, c);
        ++impl_->stats.candidates_total;
        if (c.network == CandidateNetwork::IPv4) ++impl_->stats.ipv4_candidates;
        else if (c.network == CandidateNetwork::IPv6) ++impl_->stats.ipv6_candidates;
        else if (c.network == CandidateNetwork::TorV3) ++impl_->stats.torv3_candidates;
        else if (c.network == CandidateNetwork::I2P) ++impl_->stats.i2p_candidates;
        if (c.state == CandidateState::Queued) { ++impl_->stats.queued; impl_->queue.push_back(key); }
        ++added;
    }
    if (impl_->running) impl_->pump();
    return added;
}

void PeerCrawler::add_bootstrap_seed(std::string host, std::uint16_t port) { if (port==0) port=fastseed::bsty::DEFAULT_P2P_PORT; impl_->resolve_bootstrap(std::move(host),port); }
void PeerCrawler::start() { running_=true; impl_->start(); }
void PeerCrawler::stop() { running_=false; impl_->stop(); }
CrawlerStats PeerCrawler::stats() const noexcept { return impl_->stats; }
std::size_t PeerCrawler::transport_capacity(transport::TransportKind kind) const noexcept { return impl_->capacity_controller.capacity(kind); }
std::vector<PeerCandidate> PeerCrawler::snapshot() const { std::vector<PeerCandidate> out; out.reserve(impl_->candidates.size()); for(const auto& [k,c]:impl_->candidates) out.push_back(c); return out; }

} // namespace fastseed::crawler
