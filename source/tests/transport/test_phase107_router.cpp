#include "fastseed/crawler/crawler.hpp"
#include <boost/asio.hpp>
#include <cassert>
#include <iostream>
using namespace std::chrono_literals;

using namespace fastseed;
using tcp=boost::asio::ip::tcp;

class MockTransport final : public transport::PeerTransport {
public:
    MockTransport(boost::asio::io_context& io, transport::TransportKind k):io_(io),kind_(k){}
    transport::TransportKind kind() const noexcept override{return kind_;}
    void connect(const crawler::PeerCandidate& c,std::chrono::milliseconds,transport::TransportCallback cb) override {
        ++calls;
        transport::TransportResult r; r.status=transport::TransportStatus::Unsupported; r.kind=kind_; r.endpoint=c.key(); r.error="mock";
        boost::asio::post(io_,[cb=std::move(cb),r=std::move(r)]() mutable {cb(std::move(r));});
    }
    int calls=0;
private: boost::asio::io_context& io_; transport::TransportKind kind_;
};

int main(){
    boost::asio::io_context io;
    crawler::CrawlerConfig cfg; cfg.enable_tor_transport=false; cfg.enable_i2p_transport=false;
    crawler::TransportProbeRunner runner(io,cfg);
    auto tor=std::make_shared<MockTransport>(io,transport::TransportKind::TorSocks5);
    auto i2p=std::make_shared<MockTransport>(io,transport::TransportKind::I2pSam);
    runner.router().set_tor(tor); runner.router().set_i2p(i2p);
    crawler::PeerCandidate t; t.host="onion:abcd"; t.port=8226; t.network=crawler::CandidateNetwork::TorV3; t.address_bytes.resize(32,1);
    crawler::PeerCandidate i; i.host="i2p:abcd"; i.port=8226; i.network=crawler::CandidateNetwork::I2P; i.address_bytes.resize(32,2);
    int unsupported=0;
    runner.run_candidate(t,100ms,100ms,[&](auto r){assert(r.status==protocol::ProbeStatus::TransportUnsupported);++unsupported;});
    runner.run_candidate(i,100ms,100ms,[&](auto r){assert(r.status==protocol::ProbeStatus::TransportUnsupported);++unsupported;});
    io.run();
    assert(tor->calls==1 && i2p->calls==1 && unsupported==2);
    std::cout<<"phase107_router PASS\n";
}
