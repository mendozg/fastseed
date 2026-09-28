#pragma once

#include "fastseed/crawler/candidate.hpp"
#include <boost/asio.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace fastseed::transport {

enum class TransportKind { DirectTcp, TorSocks5, I2pSam };

enum class TransportStatus { Connected, Unsupported, InvalidTarget, ConnectionFailed, HandshakeFailed, Timeout };

struct TransportResult {
    TransportStatus status{TransportStatus::ConnectionFailed};
    TransportKind kind{TransportKind::DirectTcp};
    std::string endpoint;
    std::string error;
    std::shared_ptr<boost::asio::ip::tcp::socket> socket;
};

using TransportCallback = std::function<void(TransportResult)>;

class PeerTransport {
public:
    virtual ~PeerTransport() = default;
    virtual TransportKind kind() const noexcept = 0;
    virtual void connect(const crawler::PeerCandidate& candidate,
                         std::chrono::milliseconds timeout,
                         TransportCallback done) = 0;
};

class DirectTcpTransport final : public PeerTransport {
public:
    explicit DirectTcpTransport(boost::asio::io_context& io) : io_(io) {}
    TransportKind kind() const noexcept override { return TransportKind::DirectTcp; }
    void connect(const crawler::PeerCandidate& candidate,
                 std::chrono::milliseconds timeout,
                 TransportCallback done) override;
private:
    boost::asio::io_context& io_;
};

class TorSocks5Transport final : public PeerTransport {
public:
    struct Config {
        std::string proxy_host{"127.0.0.1"};
        std::uint16_t proxy_port{9050};
    };
    explicit TorSocks5Transport(boost::asio::io_context& io) : io_(io), config_{} {}
    TorSocks5Transport(boost::asio::io_context& io, Config config)
        : io_(io), config_(std::move(config)) {}
    TransportKind kind() const noexcept override { return TransportKind::TorSocks5; }
    void connect(const crawler::PeerCandidate& candidate,
                 std::chrono::milliseconds timeout,
                 TransportCallback done) override;
private:
    boost::asio::io_context& io_;
    Config config_;
};

class I2pSamTransport final : public PeerTransport {
public:
    struct Config {
        std::string sam_host{"127.0.0.1"};
        std::uint16_t sam_port{7656};
        std::string session_id{"bsty-fastseed"};
        std::string local_nick{"fastseed"};
    };
    using DestinationResolver = std::function<std::string(const crawler::PeerCandidate&)>;

    explicit I2pSamTransport(boost::asio::io_context& io) : io_(io), config_{}, resolver_{} {}
    I2pSamTransport(boost::asio::io_context& io, Config config, DestinationResolver resolver = {})
        : io_(io), config_(std::move(config)), resolver_(std::move(resolver)) {}
    TransportKind kind() const noexcept override { return TransportKind::I2pSam; }
    void connect(const crawler::PeerCandidate& candidate,
                 std::chrono::milliseconds timeout,
                 TransportCallback done) override;
private:
    boost::asio::io_context& io_;
    Config config_;
    DestinationResolver resolver_;
};

std::string transport_name(TransportKind kind) noexcept;

class TransportRouter {
public:
    explicit TransportRouter(boost::asio::io_context& io) : io_(io) {}
    void set_direct(std::shared_ptr<PeerTransport> transport) { direct_ = std::move(transport); }
    void set_tor(std::shared_ptr<PeerTransport> transport) { tor_ = std::move(transport); }
    void set_i2p(std::shared_ptr<PeerTransport> transport) { i2p_ = std::move(transport); }
    TransportKind select(const crawler::PeerCandidate& candidate) const noexcept;
    void connect(const crawler::PeerCandidate& candidate, std::chrono::milliseconds timeout, TransportCallback done);
private:
    boost::asio::io_context& io_;
    std::shared_ptr<PeerTransport> direct_;
    std::shared_ptr<PeerTransport> tor_;
    std::shared_ptr<PeerTransport> i2p_;
};
std::string tor_v3_hostname(const crawler::PeerCandidate& candidate);

} // namespace fastseed::transport
