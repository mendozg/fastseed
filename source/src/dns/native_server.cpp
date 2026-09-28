#include "fastseed/dns/native_server.hpp"
#include <array>
#include <cstring>

namespace fastseed::dns {

class NativeDnsServer::TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    TcpSession(boost::asio::ip::tcp::socket socket, DnsResponseEngine& engine, RateLimiter& limiter)
        : socket_(std::move(socket)), engine_(engine), limiter_(limiter) {}
    void start() { read_length(); }
private:
    void read_length() {
        auto self=shared_from_this();
        boost::asio::async_read(socket_, boost::asio::buffer(length_), [self](auto ec, auto){
            if(ec) return;
            const std::size_t n=(std::size_t(self->length_[0])<<8)|self->length_[1];
            if(n < 12 || n > 4096) return;
            self->query_.resize(n);
            boost::asio::async_read(self->socket_, boost::asio::buffer(self->query_), [self](auto ec2, auto){
                if(ec2) return;
                boost::system::error_code ec3;
                const auto remote=self->socket_.remote_endpoint(ec3);
                if(ec3) return;
                if(!self->limiter_.allow(remote.address().to_string())) return;
                auto response=std::make_shared<std::vector<std::uint8_t>>(self->engine_.build_response(self->query_));
                if(response->size()>65535) return;
                self->length_[0]=static_cast<std::uint8_t>(response->size()>>8);
                self->length_[1]=static_cast<std::uint8_t>(response->size());
                self->response_=std::move(response);
                self->write_response();
            });
        });
    }
    void write_response() {
        auto self=shared_from_this();
        std::array<boost::asio::const_buffer,2> bufs{boost::asio::buffer(length_), boost::asio::buffer(*response_)};
        boost::asio::async_write(socket_, bufs, [self](auto ec, auto){ if(!ec) self->read_length(); });
    }
    boost::asio::ip::tcp::socket socket_;
    DnsResponseEngine& engine_;
    RateLimiter& limiter_;
    std::array<std::uint8_t,2> length_{};
    std::vector<std::uint8_t> query_;
    std::shared_ptr<std::vector<std::uint8_t>> response_;
};

NativeDnsServer::NativeDnsServer(boost::asio::io_context& io, DnsResponseEngine& engine)
    : NativeDnsServer(io, engine, Config{}) {}

NativeDnsServer::NativeDnsServer(boost::asio::io_context& io, DnsResponseEngine& engine, Config config)
    : io_(io), engine_(engine), config_(std::move(config)), udp_socket_(io), tcp_acceptor_(io), limiter_(config_.rate_limit) {}

void NativeDnsServer::start() {
    if(started_) return;
    started_=true;
    if(config_.enable_udp) start_udp();
    if(config_.enable_tcp) start_tcp();
}

bool NativeDnsServer::running() const noexcept { return started_ && (udp_socket_.is_open() || tcp_acceptor_.is_open()); }

void NativeDnsServer::stop() {
    started_=false;
    boost::system::error_code ec;
    udp_socket_.cancel(ec); udp_socket_.close(ec);
    tcp_acceptor_.cancel(ec); tcp_acceptor_.close(ec);
}

void NativeDnsServer::start_udp() {
    boost::system::error_code ec;
    const auto proto=config_.bind_address.is_v6()?boost::asio::ip::udp::v6():boost::asio::ip::udp::v4();
    udp_socket_.open(proto,ec); if(ec) throw boost::system::system_error(ec);
    udp_socket_.set_option(boost::asio::socket_base::reuse_address(true),ec);
    udp_socket_.bind({config_.bind_address,config_.port},ec); if(ec) throw boost::system::system_error(ec);
    receive_udp();
}

void NativeDnsServer::receive_udp() {
    udp_socket_.async_receive_from(boost::asio::buffer(udp_buffer_),udp_remote_,[this](auto ec,std::size_t n){
        if(!ec && n>=12) {
            const auto source=udp_remote_.address().to_string();
            if(limiter_.allow(source)) {
                auto response=std::make_shared<std::vector<std::uint8_t>>(engine_.build_response(std::span<const std::uint8_t>(udp_buffer_.data(),n)));
                if(!response->empty()) send_udp(std::move(response),udp_remote_);
                else if(started_) receive_udp();
            } else if(started_) receive_udp();
        } else if(started_) receive_udp();
    });
}

void NativeDnsServer::send_udp(std::shared_ptr<std::vector<std::uint8_t>> response, boost::asio::ip::udp::endpoint remote) {
    udp_socket_.async_send_to(boost::asio::buffer(*response),remote,[this,response](auto,auto){ if(started_) receive_udp(); });
}

void NativeDnsServer::start_tcp() {
    boost::system::error_code ec;
    const auto proto=config_.bind_address.is_v6()?boost::asio::ip::tcp::v6():boost::asio::ip::tcp::v4();
    tcp_acceptor_.open(proto,ec); if(ec) throw boost::system::system_error(ec);
    tcp_acceptor_.set_option(boost::asio::socket_base::reuse_address(true),ec);
    tcp_acceptor_.bind({config_.bind_address,config_.port},ec); if(ec) throw boost::system::system_error(ec);
    tcp_acceptor_.listen(boost::asio::socket_base::max_listen_connections,ec); if(ec) throw boost::system::system_error(ec);
    accept_tcp();
}

void NativeDnsServer::accept_tcp() {
    tcp_acceptor_.async_accept([this](auto ec, auto socket){
        if(!ec && started_) std::make_shared<TcpSession>(std::move(socket),engine_,limiter_)->start();
        if(started_) accept_tcp();
    });
}

} // namespace fastseed::dns
