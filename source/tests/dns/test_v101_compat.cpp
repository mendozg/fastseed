#include "fastseed/dns/native_server.hpp"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <thread>
using namespace fastseed;
using Bytes=std::vector<std::uint8_t>;
static int checks=0;
static void check(bool v,const char* label){++checks;if(!v)throw std::runtime_error(label);}
static unsigned r16(const Bytes& v,int p){return (v.at(p)<<8)|v.at(p+1);}
static Bytes query(unsigned type=1,std::string name="seed.globalboost.info"){
 Bytes v{0x12,0x34,1,0,0,1,0,0,0,0,0,0};
 size_t s=0;while(s<name.size()){auto e=name.find('.',s);if(e==std::string::npos)e=name.size();v.push_back(e-s);v.insert(v.end(),name.begin()+s,name.begin()+e);s=e+1;}
 v.insert(v.end(),{0,0,static_cast<std::uint8_t>(type),0,1});return v;
}
static void opt(Bytes& q,unsigned size=1232,unsigned version=0,Bytes options={}){
 ++q[11];q.insert(q.end(),{0,0,41,static_cast<std::uint8_t>(size>>8),static_cast<std::uint8_t>(size),0,static_cast<std::uint8_t>(version),0,0,static_cast<std::uint8_t>(options.size()>>8),static_cast<std::uint8_t>(options.size())});q.insert(q.end(),options.begin(),options.end());
}
int main(){try{
 dns::DnsConfig cfg;cfg.max_answers=100;dns::DnsResponseEngine e(cfg);health::PeerHealthCache cache;
 protocol::ProbeResult ok{};ok.status=protocol::ProbeStatus::Success;ok.protocol_version=70016;ok.user_agent="/BSTYFastSeed/";
 for(int n=1;n<=80;++n){crawler::PeerCandidate c{"192.0.2."+std::to_string(n),8226,crawler::CandidateSource::Manual};for(int i=0;i<3;++i)cache.observe_success(c,ok);}
 crawler::PeerCandidate c6{"2001:db8::1",8226,crawler::CandidateSource::Manual};for(int i=0;i<3;++i)cache.observe_success(c6,ok);
 e.publish_from_cache(cache,1);
 auto mixed=query(28,"SeEd.GlObAlBoOsT.InFo");auto mixed_response=e.build_response(mixed);check(std::equal(mixed.begin()+12,mixed.end(),mixed_response.begin()+12),"question case preserved");
 auto plain=query();auto r=e.build_response(plain);check(r16(r,2)==0x8700,"legacy TC bit and flags");check(r.size()<=512 && r16(r,6)==0,"legacy UDP bound and complete RRset");
 auto q=query();opt(q);r=e.build_response(q);check((r16(r,2)&0x0200)!=0 && r.size()<=1232,"EDNS UDP bound");check(r16(r,10)==1 && r16(r,r.size()-10)==41,"OPT record");
 auto tcp=e.build_response(q,true);check(r16(tcp,6)==80 && !(r16(tcp,2)&0x0200),"TCP full answers");check(tcp.size()>1232,"TCP bypass UDP cap");
 auto small=query();opt(small,128);r=e.build_response(small);check(r.size()<=512 && (r16(r,2)&0x200),"EDNS below 512");
 auto big=query();opt(big,4096);r=e.build_response(big);check(r.size()<=1232 && (r16(r,2)&0x200),"server cap");
 auto aaaa=query(28);opt(aaaa);r=e.build_response(aaaa);check(r16(r,6)==1 && !(r16(r,2)&0x200),"EDNS AAAA");
 for(auto type:{2,6}) {r=e.build_response(query(type));check(r16(r,6)==1 && (r16(r,2)&0x400),"authoritative NS/SOA");}
 r=e.build_response(query(16));check(r16(r,6)==0 && r16(r,8)==1 && (r16(r,2)&15)==0,"NODATA with SOA");
 dns::DnsResponseEngine empty; r=empty.build_response(query());check(r16(r,8)==1 && (r16(r,2)&15)==0,"empty cache SOA");
 r=e.build_response(query(1,"child.seed.globalboost.info"));check((r16(r,2)&15)==3 && r16(r,8)==1,"NXDOMAIN SOA");
 r=e.build_response(query(1,"other.example"));check((r16(r,2)&0x40f)==5,"REFUSED outside zone without AA");
 auto doq=query(28);opt(doq);doq[doq.size()-4]=0x80;r=e.build_response(doq);check(r[r.size()-4]==0x80,"DO copied without DNSSEC records");
 auto v=query();opt(v,1232,1);r=e.build_response(v);check((r16(r,2)&15)==0 && r[r.size()-6]==1 && r[r.size()-5]==0,"BADVERS extended rcode/version");
 v=query();opt(v,1232,0,{0,10,0,2,42,43});r=e.build_response(v,true);check((r16(r,2)&15)==0,"unknown EDNS options ignored");
 v=query();opt(v);opt(v);r=e.build_response(v);check((r16(r,2)&15)==1 && r16(r,10)==0,"duplicate OPT FORMERR");
 v=query();opt(v,1232,0,{0,10,0,4,42});r=e.build_response(v);check((r16(r,2)&15)==1,"option length FORMERR");
 v=query();opt(v);v.pop_back();r=e.build_response(v);check((r16(r,2)&15)==1,"truncated OPT FORMERR");
 v=query();v.push_back(42);r=e.build_response(v);check((r16(r,2)&15)==1,"trailing junk FORMERR");
 v=query();v[2]=0x29;r=e.build_response(v);check((r16(r,2)&15)==4 && (r16(r,2)&0x7800)==0x2800,"opcode NOTIMP");
 v=query();v[2]|=0x80;check(e.build_response(v).empty(),"QR silently discarded");
 v=Bytes(12,0);v[5]=1;v.insert(v.end(),{0xc0,12,0,1,0,1});r=e.build_response(v);check((r16(r,2)&15)==1 && r16(r,4)==0 && r.size()==12,"compression loop FORMERR");
 check(e.build_response(Bytes(11,0)).empty(),"short packet discard");
 for(size_t n=12;n<q.size();++n){Bytes cut(q.begin(),q.begin()+n);auto resp=e.build_response(cut);check(!resp.empty() && (r16(resp,2)&15)==1,"all truncated EDNS prefixes");}
 // Exercise the real UDP and persistent TCP listeners on a kernel-selected free port.
 boost::asio::io_context io; boost::asio::ip::tcp::acceptor probe(io,{boost::asio::ip::address_v4::loopback(),0});auto port=probe.local_endpoint().port();probe.close();
 dns::NativeDnsServer::Config sc;sc.port=port;sc.bind_address=boost::asio::ip::address_v4::loopback();dns::NativeDnsServer server(io,e,sc);server.start();
 std::thread worker([&]{io.run();});
 try {
  boost::asio::io_context client;boost::asio::ip::udp::socket udp(client,boost::asio::ip::udp::v4());udp.send_to(boost::asio::buffer(aaaa),{sc.bind_address,port});Bytes buffer(4096);boost::asio::ip::udp::endpoint remote;auto len=udp.receive_from(boost::asio::buffer(buffer),remote);buffer.resize(len);check(buffer==e.build_response(aaaa),"real UDP EDNS bytes");
  for(const auto& request:{q,plain,doq}) {buffer.resize(4096);udp.send_to(boost::asio::buffer(request),{sc.bind_address,port});len=udp.receive_from(boost::asio::buffer(buffer),remote);buffer.resize(len);check(buffer==e.build_response(request),"real UDP negotiation/truncation bytes");}
  boost::asio::ip::tcp::socket sock(client);sock.connect({sc.bind_address,port});
  for(const auto& request:{q,aaaa,v}) { // v is a malformed name; verify the session survives FORMERR.
   Bytes frame{static_cast<std::uint8_t>(request.size()>>8),static_cast<std::uint8_t>(request.size())};frame.insert(frame.end(),request.begin(),request.end());boost::asio::write(sock,boost::asio::buffer(frame));
   std::array<std::uint8_t,2> length;boost::asio::read(sock,boost::asio::buffer(length));Bytes reply((length[0]<<8)|length[1]);boost::asio::read(sock,boost::asio::buffer(reply));check(reply==e.build_response(request,true),"real persistent TCP response bytes");
  }
  sock.close();
 } catch(...) {io.stop();worker.join();throw;}
 io.stop();worker.join();server.stop();
 std::cout<<"DNS v1.0.0 compatibility: "<<checks<<" checks PASS\n";
 }catch(const std::exception& ex){std::cerr<<ex.what()<<"\n";return 1;}}

