#pragma once
#include "fastseed/dns/dns_engine.hpp"
#include <boost/asio.hpp>
#include <memory>

namespace fastseed::dns {

class NativeDnsServer {
public:
    struct Config {
        std::uint16_t port{53};
        boost::asio::ip::address bind_address{boost::asio::ip::address_v4::any()};
        bool enable_udp{true};
        bool enable_tcp{true};
        RateLimiter::Config rate_limit{};
    };

    NativeDnsServer(boost::asio::io_context& io, DnsResponseEngine& engine);
    NativeDnsServer(boost::asio::io_context& io, DnsResponseEngine& engine, Config config);
    void start();
    void stop();
    bool running() const noexcept;
    std::uint16_t port() const noexcept { return config_.port; }

private:
    class TcpSession;
    void start_udp();
    void receive_udp();
    void start_tcp();
    void accept_tcp();
    void send_udp(std::shared_ptr<std::vector<std::uint8_t>> response,
                  boost::asio::ip::udp::endpoint remote);

    boost::asio::io_context& io_;
    DnsResponseEngine& engine_;
    Config config_;
    boost::asio::ip::udp::socket udp_socket_;
    boost::asio::ip::udp::endpoint udp_remote_;
    std::array<std::uint8_t, 4096> udp_buffer_{};
    boost::asio::ip::tcp::acceptor tcp_acceptor_;
    RateLimiter limiter_;
    bool started_{false};
};

} // namespace fastseed::dns
