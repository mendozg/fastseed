#include "fastseed/transport/transport.hpp"
#include <boost/asio.hpp>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
using namespace fastseed;
using tcp=boost::asio::ip::tcp;

static crawler::PeerCandidate tor_candidate(){ crawler::PeerCandidate c; c.network=crawler::CandidateNetwork::TorV3; c.host="onion:"; c.port=8226; c.address_bytes.resize(32); for(size_t i=0;i<32;++i)c.address_bytes[i]=static_cast<unsigned char>(i); c.state=crawler::CandidateState::AwaitingTransport; return c; }
static crawler::PeerCandidate i2p_candidate(){ crawler::PeerCandidate c; c.network=crawler::CandidateNetwork::I2P; c.host="i2p:00"; c.port=8226; c.address_bytes.resize(32,0x42); c.state=crawler::CandidateState::AwaitingTransport; return c; }

int main(){
    auto tor=tor_candidate(); auto onion=transport::tor_v3_hostname(tor); assert(onion.size()==62); assert(onion.substr(onion.size()-6)==".onion"); assert(onion.find('A')==std::string::npos);
    boost::asio::io_context io;
    transport::TorSocks5Transport tortr(io,{"127.0.0.1",1});
    bool called=false; transport::TransportResult r;
    tortr.connect(tor,std::chrono::milliseconds(50),[&](auto x){called=true;r=std::move(x);}); io.run(); assert(called); assert(r.status==transport::TransportStatus::ConnectionFailed || r.status==transport::TransportStatus::Timeout);
    io.restart();
    transport::I2pSamTransport i2p(io,{},[](const crawler::PeerCandidate&){return std::string("BASE64DEST");});
    called=false; i2p.connect(i2p_candidate(),std::chrono::milliseconds(10),[&](auto x){called=true;r=std::move(x);}); io.run(); assert(called); assert(r.status==transport::TransportStatus::ConnectionFailed || r.status==transport::TransportStatus::Timeout);
    std::cout<<"phase10.6 transport unit tests PASS\n";
}
