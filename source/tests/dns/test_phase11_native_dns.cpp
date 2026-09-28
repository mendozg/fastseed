#include "fastseed/dns/native_server.hpp"
#include <boost/asio.hpp>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
using namespace fastseed;
static std::vector<std::uint8_t> q(std::uint16_t id,const char* name,std::uint16_t type){std::vector<std::uint8_t> x{std::uint8_t(id>>8),std::uint8_t(id),1,0,0,1,0,0,0,0,0,0};std::size_t s=0;std::string n(name);while(s<n.size()){auto d=n.find('.',s);auto e=d==std::string::npos?n.size():d;x.push_back(std::uint8_t(e-s));x.insert(x.end(),n.begin()+s,n.begin()+e);if(d==std::string::npos)break;s=d+1;}x.push_back(0);x.push_back(type>>8);x.push_back(type);x.push_back(0);x.push_back(1);return x;}
int main(){boost::asio::io_context io;dns::DnsConfig dc;dc.zone="seed.globalboost.info";dc.max_answers=8;dc.max_response_size=1232;dns::DnsResponseEngine engine(dc);health::PeerHealthCache cache;protocol::ProbeResult ok{};ok.status=protocol::ProbeStatus::Success;ok.protocol_version=70016;ok.user_agent="/BSTYFastSeed/";for(auto h:{"127.0.0.2","127.0.0.3"}){crawler::PeerCandidate c{h,8226,crawler::CandidateSource::Manual};cache.observe_success(c,ok);cache.observe_success(c,ok);cache.observe_success(c,ok);}engine.publish_from_cache(cache,11);dns::NativeDnsServer::Config cfg;cfg.port=0;cfg.enable_udp=false;cfg.enable_tcp=false;dns::NativeDnsServer server(io,engine,cfg);assert(!server.running());server.start();server.stop();auto r=engine.build_response(q(0x1234,"seed.globalboost.info",dns::TYPE_A));assert(r.size()>12);assert(r[0]==0x12&&r[1]==0x34);assert(r[7]==2);std::cout<<"Phase 11 native DNS tests PASS\n";}
