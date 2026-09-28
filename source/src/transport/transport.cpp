#include "fastseed/transport/transport.hpp"
#include "fastseed/protocol/addrv2.hpp"
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <openssl/evp.h>
#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace fastseed::transport {
namespace {
using tcp = boost::asio::ip::tcp;

std::string endpoint_name(const crawler::PeerCandidate& c) {
    return std::string(crawler::to_string(c.network)) + ":" + c.host + ":" + std::to_string(c.port);
}

bool is_tor(const crawler::PeerCandidate& c) { return c.network == crawler::CandidateNetwork::TorV3; }
bool is_i2p(const crawler::PeerCandidate& c) { return c.network == crawler::CandidateNetwork::I2P; }

std::string b32(std::span<const std::uint8_t> in) {
    static constexpr char alphabet[] = "abcdefghijklmnopqrstuvwxyz234567";
    std::string out;
    unsigned buffer = 0; int bits = 0;
    for (auto b : in) {
        buffer = (buffer << 8) | b; bits += 8;
        while (bits >= 5) { bits -= 5; out.push_back(alphabet[(buffer >> bits) & 31u]); }
    }
    if (bits) out.push_back(alphabet[(buffer << (5 - bits)) & 31u]);
    return out;
}

std::vector<std::uint8_t> tor_v3_encoded(std::span<const std::uint8_t> pubkey) {
    if (pubkey.size() != 32) throw std::invalid_argument("Tor v3 ADDRv2 address must be 32 bytes");
    std::array<std::uint8_t, 35> raw{};
    std::copy(pubkey.begin(), pubkey.end(), raw.begin());
    raw[34] = 3;
    static constexpr char prefix[] = ".onion checksum";
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");
    unsigned char digest[32]{}; unsigned int len = 0;
    if (EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, prefix, sizeof(prefix) - 1) != 1 ||
        EVP_DigestUpdate(ctx, pubkey.data(), pubkey.size()) != 1 ||
        EVP_DigestUpdate(ctx, raw.data() + 34, 1) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &len) != 1) {
        EVP_MD_CTX_free(ctx); throw std::runtime_error("SHA3-256 failed");
    }
    EVP_MD_CTX_free(ctx);
    raw[32] = digest[0]; raw[33] = digest[1];
    return {raw.begin(), raw.end()};
}

void finish(const std::shared_ptr<tcp::socket>& sock, const std::shared_ptr<boost::asio::steady_timer>& timer,
            TransportResult result, TransportCallback cb) {
    boost::system::error_code ec;
    timer->cancel(ec);
    // A successfully established transport socket is handed to PeerProbe.
    // Do not cancel it here; cancellation would abort the first BSTY handshake read/write.
    if (result.status != TransportStatus::Connected && sock) sock->cancel(ec);
    if (cb) cb(std::move(result));
}

void direct_connect(boost::asio::io_context& io, const crawler::PeerCandidate& c,
                    std::chrono::milliseconds timeout, TransportCallback cb) {
    auto sock = std::make_shared<tcp::socket>(io);
    auto resolver = std::make_shared<tcp::resolver>(io);
    auto timer = std::make_shared<boost::asio::steady_timer>(io);
    auto done = std::make_shared<bool>(false);
    auto complete = [sock,timer,done,cb,c](TransportResult r) mutable {
        if (*done) return;
        *done = true;
        finish(sock,timer,std::move(r),std::move(cb));
    };
    timer->expires_after(timeout);
    timer->async_wait([sock,done,complete](boost::system::error_code ec) mutable {
        if (ec || *done) return;
        boost::system::error_code x;
        sock->cancel(x);
        sock->close(x);
        TransportResult r; r.status=TransportStatus::Timeout; r.kind=TransportKind::DirectTcp; r.error="transport timeout"; complete(std::move(r));
    });
    resolver->async_resolve(c.host, std::to_string(c.port),
        [sock,resolver,complete](boost::system::error_code ec, tcp::resolver::results_type results) mutable {
            if (ec) { TransportResult r; r.status=TransportStatus::ConnectionFailed; r.kind=TransportKind::DirectTcp; r.error=ec.message(); complete(std::move(r)); return; }
            boost::asio::async_connect(*sock, results, [sock,complete](boost::system::error_code ec, const tcp::endpoint&) mutable {
                TransportResult r; r.kind=TransportKind::DirectTcp;
                if (ec) { r.status=TransportStatus::ConnectionFailed; r.error=ec.message(); }
                else { r.status=TransportStatus::Connected; r.socket=sock; }
                complete(std::move(r));
            });
        });
}

} // namespace

TransportKind TransportRouter::select(const crawler::PeerCandidate& c) const noexcept {
    switch (c.network) {
        case crawler::CandidateNetwork::IPv4:
        case crawler::CandidateNetwork::IPv6: return TransportKind::DirectTcp;
        case crawler::CandidateNetwork::TorV3: return TransportKind::TorSocks5;
        case crawler::CandidateNetwork::I2P: return TransportKind::I2pSam;
        default: return TransportKind::DirectTcp;
    }
}

void TransportRouter::connect(const crawler::PeerCandidate& c, std::chrono::milliseconds timeout, TransportCallback done) {
    std::shared_ptr<PeerTransport> selected;
    switch (select(c)) {
        case TransportKind::DirectTcp: selected = direct_; break;
        case TransportKind::TorSocks5: selected = tor_; break;
        case TransportKind::I2pSam: selected = i2p_; break;
    }
    if (!selected) {
        TransportResult r; r.kind = select(c); r.status = TransportStatus::Unsupported; r.endpoint = endpoint_name(c); r.error = "transport adapter is not configured";
        boost::asio::post(io_, [done = std::move(done), r = std::move(r)]() mutable { done(std::move(r)); });
        return;
    }
    selected->connect(c, timeout, std::move(done));
}

std::string transport_name(TransportKind k) noexcept {
    switch (k) { case TransportKind::DirectTcp:return "direct_tcp"; case TransportKind::TorSocks5:return "tor_socks5"; case TransportKind::I2pSam:return "i2p_sam"; }
    return "unknown";
}

std::string tor_v3_hostname(const crawler::PeerCandidate& c) {
    if (!is_tor(c) || c.address_bytes.size() != 32) throw std::invalid_argument("invalid Tor v3 candidate");
    auto raw = tor_v3_encoded(c.address_bytes);
    return b32(raw) + ".onion";
}

void DirectTcpTransport::connect(const crawler::PeerCandidate& c, std::chrono::milliseconds timeout, TransportCallback done) {
    if (!c.transport_probe_supported()) { TransportResult r; r.status=TransportStatus::Unsupported; r.kind=kind(); r.endpoint=endpoint_name(c); r.error="direct TCP supports only IPv4/IPv6"; done(std::move(r)); return; }
    direct_connect(io_, c, timeout, std::move(done));
}

void TorSocks5Transport::connect(const crawler::PeerCandidate& c, std::chrono::milliseconds timeout, TransportCallback cb) {
    TransportResult base; base.kind=kind(); base.endpoint=endpoint_name(c);
    if (!is_tor(c) || c.address_bytes.size()!=32 || c.port==0) { base.status=TransportStatus::InvalidTarget; base.error="invalid Tor v3 candidate"; cb(std::move(base)); return; }
    auto target = tor_v3_hostname(c);
    auto sock = std::make_shared<tcp::socket>(io_);
    auto resolver = std::make_shared<tcp::resolver>(io_);
    auto timer = std::make_shared<boost::asio::steady_timer>(io_);
    auto done = std::make_shared<bool>(false);
    auto complete = [sock,timer,done,cb,base](TransportResult r) mutable { if(*done)return; *done=true; finish(sock,timer,std::move(r),std::move(cb)); };
    timer->expires_after(timeout);
    timer->async_wait([sock,done,complete](boost::system::error_code ec) mutable { if(ec||*done)return; boost::system::error_code x; sock->cancel(x); sock->close(x); TransportResult r; r.status=TransportStatus::Timeout; r.kind=TransportKind::TorSocks5; r.error="SOCKS5 timeout"; complete(std::move(r)); });
    resolver->async_resolve(config_.proxy_host,std::to_string(config_.proxy_port),[sock,resolver,target,c,complete](boost::system::error_code ec,tcp::resolver::results_type rr) mutable {
        if(ec){ TransportResult r; r.status=TransportStatus::ConnectionFailed; r.kind=TransportKind::TorSocks5; r.error=ec.message(); complete(std::move(r)); return; }
        boost::asio::async_connect(*sock,rr,[sock,target,c,complete](boost::system::error_code ec,const tcp::endpoint&) mutable {
            if(ec){ TransportResult r; r.status=TransportStatus::ConnectionFailed; r.kind=TransportKind::TorSocks5; r.error=ec.message(); complete(std::move(r)); return; }
            auto hello=std::make_shared<std::array<std::uint8_t,3>>(std::array<std::uint8_t,3>{5,1,0});
            boost::asio::async_write(*sock,boost::asio::buffer(*hello),[sock,target,c,complete,hello](boost::system::error_code ec,std::size_t) mutable {
                if(ec){ TransportResult r; r.status=TransportStatus::HandshakeFailed; r.kind=TransportKind::TorSocks5; r.error=ec.message(); complete(std::move(r)); return; }
                auto resp=std::make_shared<std::array<std::uint8_t,2>>();
                boost::asio::async_read(*sock,boost::asio::buffer(*resp),[sock,target,c,complete,resp](boost::system::error_code ec,std::size_t) mutable {
                    if(ec || (*resp)[0]!=5 || (*resp)[1]!=0){ TransportResult r; r.status=TransportStatus::HandshakeFailed; r.kind=TransportKind::TorSocks5; r.error=ec?ec.message():"SOCKS5 no-auth method rejected"; complete(std::move(r)); return; }
                    if(target.size()>255){ TransportResult r; r.status=TransportStatus::InvalidTarget; r.kind=TransportKind::TorSocks5; r.error="SOCKS5 hostname too long"; complete(std::move(r)); return; }
                    auto req=std::make_shared<std::vector<std::uint8_t>>(); req->reserve(7+target.size()); req->insert(req->end(),{5,1,0,3,static_cast<std::uint8_t>(target.size())}); req->insert(req->end(),target.begin(),target.end()); req->push_back(static_cast<std::uint8_t>(c.port>>8)); req->push_back(static_cast<std::uint8_t>(c.port));
                    boost::asio::async_write(*sock,boost::asio::buffer(*req),[sock,complete,req](boost::system::error_code ec,std::size_t) mutable {
                        if(ec){ TransportResult r; r.status=TransportStatus::HandshakeFailed; r.kind=TransportKind::TorSocks5; r.error=ec.message(); complete(std::move(r)); return; }
                        auto head=std::make_shared<std::array<std::uint8_t,4>>(); boost::asio::async_read(*sock,boost::asio::buffer(*head),[sock,complete,head](boost::system::error_code ec,std::size_t) mutable {
                            if(ec || (*head)[0]!=5 || (*head)[1]!=0){ TransportResult r; r.status=TransportStatus::HandshakeFailed; r.kind=TransportKind::TorSocks5; r.error=ec?ec.message():"SOCKS5 CONNECT rejected"; complete(std::move(r)); return; }
                            const std::size_t n=((*head)[3]==1?4:(*head)[3]==4?16:(*head)[3]==3?1:0); if(!n){TransportResult r; r.status=TransportStatus::HandshakeFailed;r.kind=TransportKind::TorSocks5;r.error="unsupported SOCKS5 bound address type";complete(std::move(r));return;}
                            auto tail=std::make_shared<std::vector<std::uint8_t>>(n+2); boost::asio::async_read(*sock,boost::asio::buffer(*tail),[sock,complete,head,tail](boost::system::error_code ec,std::size_t) mutable { TransportResult r; r.kind=TransportKind::TorSocks5; if(ec){r.status=TransportStatus::HandshakeFailed;r.error=ec.message();}else{r.status=TransportStatus::Connected;r.socket=sock;} complete(std::move(r)); });
                        });
                    });
                });
            });
        });
    });
}

void I2pSamTransport::connect(const crawler::PeerCandidate& c, std::chrono::milliseconds timeout, TransportCallback cb) {
    TransportResult base; base.kind=kind(); base.endpoint=endpoint_name(c);
    if (!is_i2p(c) || c.address_bytes.size()!=32 || c.port==0) { base.status=TransportStatus::InvalidTarget; base.error="invalid I2P ADDRv2 candidate"; cb(std::move(base)); return; }
    if (!resolver_) { base.status=TransportStatus::Unsupported; base.error="I2P destination resolver is not configured"; cb(std::move(base)); return; }
    auto destination=resolver_(c); if(destination.empty()){base.status=TransportStatus::InvalidTarget;base.error="I2P destination not resolved";cb(std::move(base));return;}
    auto sock=std::make_shared<tcp::socket>(io_); auto resolver=std::make_shared<tcp::resolver>(io_); auto timer=std::make_shared<boost::asio::steady_timer>(io_); auto done=std::make_shared<bool>(false);
    auto complete=[sock,timer,done,cb,base](TransportResult r) mutable {if(*done)return;*done=true;finish(sock,timer,std::move(r),std::move(cb));};
    timer->expires_after(timeout); timer->async_wait([sock,done,complete](boost::system::error_code ec) mutable {if(ec||*done)return;boost::system::error_code x;sock->cancel(x);sock->close(x);TransportResult r;r.status=TransportStatus::Timeout;r.kind=TransportKind::I2pSam;r.error="SAM timeout";complete(std::move(r));});
    resolver->async_resolve(config_.sam_host,std::to_string(config_.sam_port),[sock,resolver,complete,this,destination](boost::system::error_code ec,tcp::resolver::results_type rr) mutable {
        if(ec){TransportResult r;r.status=TransportStatus::ConnectionFailed;r.kind=TransportKind::I2pSam;r.error=ec.message();complete(std::move(r));return;}
        boost::asio::async_connect(*sock,rr,[sock,complete,this,destination](boost::system::error_code ec,const tcp::endpoint&) mutable {
            if(ec){TransportResult r;r.status=TransportStatus::ConnectionFailed;r.kind=TransportKind::I2pSam;r.error=ec.message();complete(std::move(r));return;}
            auto write_line=[sock](const std::string& s,auto next){boost::asio::async_write(*sock,boost::asio::buffer(s),std::move(next));};
            auto greeting=std::string("HELLO VERSION MIN=3.1 MAX=3.3\n");
            write_line(greeting,[sock,complete,this,destination](boost::system::error_code ec,std::size_t) mutable {
                if(ec){TransportResult r;r.status=TransportStatus::HandshakeFailed;r.kind=TransportKind::I2pSam;r.error=ec.message();complete(std::move(r));return;}
                auto buf=std::make_shared<boost::asio::streambuf>(); boost::asio::async_read_until(*sock,*buf,'\n',[sock,buf,complete,this,destination](boost::system::error_code ec,std::size_t) mutable {
                    if(ec){TransportResult r;r.status=TransportStatus::HandshakeFailed;r.kind=TransportKind::I2pSam;r.error=ec.message();complete(std::move(r));return;}
                    std::istream in(buf.get()); std::string line; std::getline(in,line); if(line.find("HELLO REPLY RESULT=OK")!=0){TransportResult r;r.status=TransportStatus::HandshakeFailed;r.kind=TransportKind::I2pSam;r.error="SAM HELLO rejected";complete(std::move(r));return;}
                    std::string session="SESSION CREATE STYLE=STREAM ID="+config_.session_id+" DESTINATION=TRANSIENT SIGNATURE_TYPE=7\n";
                    boost::asio::async_write(*sock,boost::asio::buffer(session),[sock,complete,this,destination](boost::system::error_code ec,std::size_t) mutable {
                        if(ec){TransportResult r;r.status=TransportStatus::HandshakeFailed;r.kind=TransportKind::I2pSam;r.error=ec.message();complete(std::move(r));return;}
                        auto b=std::make_shared<boost::asio::streambuf>(); boost::asio::async_read_until(*sock,*b,'\n',[sock,b,complete,this,destination](boost::system::error_code ec,std::size_t) mutable {
                            if(ec){TransportResult r;r.status=TransportStatus::HandshakeFailed;r.kind=TransportKind::I2pSam;r.error=ec.message();complete(std::move(r));return;}
                            std::istream in(b.get());std::string line;std::getline(in,line);if(line.find("SESSION STATUS RESULT=OK")!=0){TransportResult r;r.status=TransportStatus::HandshakeFailed;r.kind=TransportKind::I2pSam;r.error="SAM session rejected";complete(std::move(r));return;}
                            std::string connect="STREAM CONNECT ID="+config_.session_id+" DESTINATION="+destination+" SILENT=false\n";
                            boost::asio::async_write(*sock,boost::asio::buffer(connect),[sock,complete](boost::system::error_code ec,std::size_t) mutable {
                                if(ec){TransportResult r;r.status=TransportStatus::HandshakeFailed;r.kind=TransportKind::I2pSam;r.error=ec.message();complete(std::move(r));return;}
                                auto b=std::make_shared<boost::asio::streambuf>();boost::asio::async_read_until(*sock,*b,'\n',[sock,b,complete](boost::system::error_code ec,std::size_t) mutable {TransportResult r;r.kind=TransportKind::I2pSam;if(ec){r.status=TransportStatus::HandshakeFailed;r.error=ec.message();}else{std::istream in(b.get());std::string line;std::getline(in,line);if(line.find("STREAM STATUS RESULT=OK")!=0){r.status=TransportStatus::HandshakeFailed;r.error="SAM stream rejected";}else{r.status=TransportStatus::Connected;r.socket=sock;}}complete(std::move(r));});
                            });
                        });
                    });
                });
            });
        });
    });
}

} // namespace fastseed::transport
