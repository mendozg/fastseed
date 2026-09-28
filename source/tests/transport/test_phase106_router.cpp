#include "fastseed/transport/transport.hpp"
#include <boost/asio.hpp>
#include <cassert>
#include <iostream>
using namespace fastseed;
struct Stub final : transport::PeerTransport { transport::TransportKind k; explicit Stub(transport::TransportKind x):k(x){} transport::TransportKind kind() const noexcept override{return k;} void connect(const crawler::PeerCandidate&,std::chrono::milliseconds,transport::TransportCallback cb) override{transport::TransportResult r;r.status=transport::TransportStatus::Connected;r.kind=k;cb(std::move(r));}};
int main(){boost::asio::io_context io;transport::TransportRouter r(io);r.set_direct(std::make_shared<Stub>(transport::TransportKind::DirectTcp));r.set_tor(std::make_shared<Stub>(transport::TransportKind::TorSocks5));r.set_i2p(std::make_shared<Stub>(transport::TransportKind::I2pSam));crawler::PeerCandidate c;c.network=crawler::CandidateNetwork::TorV3;assert(r.select(c)==transport::TransportKind::TorSocks5);bool ok=false;r.connect(c,std::chrono::milliseconds(1),[&](auto x){ok=x.status==transport::TransportStatus::Connected&&x.kind==transport::TransportKind::TorSocks5;});assert(!ok);io.run();assert(ok);c.network=crawler::CandidateNetwork::I2P;assert(r.select(c)==transport::TransportKind::I2pSam);c.network=crawler::CandidateNetwork::IPv6;assert(r.select(c)==transport::TransportKind::DirectTcp);std::cout<<"phase10.6 transport router PASS\n";}
