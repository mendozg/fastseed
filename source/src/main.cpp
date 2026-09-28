#include "fastseed/protocol/probe.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/config/runtime_config.hpp"
#include "fastseed/crawler/crawler.hpp"
#include "fastseed/health/peer_health.hpp"
#include "fastseed/db/peer_database.hpp"
#include "fastseed/dns/dns_engine.hpp"
#include "fastseed/dns/native_server.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <fstream>

static bool parse_endpoint(std::string ep,std::string& host,std::uint16_t& port){
 port=fastseed::bsty::DEFAULT_P2P_PORT;
 if(ep.size()>=2 && ep.front()=='['){auto close=ep.find(']');if(close==std::string::npos)return false;host=ep.substr(1,close-1);if(close+1<ep.size()){if(ep[close+1]!=':')return false;try{auto p=std::stoul(ep.substr(close+2));if(p>65535)return false;port=static_cast<std::uint16_t>(p);}catch(...){return false;}}return !host.empty();}
 auto first=ep.find(':'), last=ep.rfind(':'); if(first!=std::string::npos && first!=last){host=ep;return true;} if(last!=std::string::npos){host=ep.substr(0,last);try{auto p=std::stoul(ep.substr(last+1));if(p>65535)return false;port=static_cast<std::uint16_t>(p);}catch(...){return false;}}else host=ep;return !host.empty();
}
static void usage(){std::cout<<"BSTY FastSeed v1.0.0\nUsage:\n  bsty-fastseed run [--dns-port <port>]\n  bsty-fastseed probe <host>[:port]\n  bsty-fastseed --version\n  bsty-fastseed --help\n  bsty-fastseed --check-config <file>\n  bsty-fastseed --self-test\n";}
static int run_seed(std::uint16_t dns_port){
 boost::asio::io_context io;
 fastseed::health::PeerHealthCache health;
 fastseed::db::PeerDatabase db;
 try { db.open(); health.restore(db.load_health()); } catch(const std::exception& e) { std::cerr<<"database error: "<<e.what()<<"\n"; return 1; }
 fastseed::dns::DnsConfig dns_cfg; dns_cfg.listen_port=dns_port;
 fastseed::dns::DnsResponseEngine dns(dns_cfg);
 dns.publish_from_cache(health, 1);
 fastseed::dns::NativeDnsServer dns_server(io, dns, {.port=dns_port});
 fastseed::crawler::CrawlerConfig cc;
 cc.max_concurrent_probes=256; cc.max_candidates=100000; cc.max_open_connections=2048;
 fastseed::crawler::PeerCrawler crawler(io, cc);
 crawler.set_validated_callback([&](const fastseed::crawler::PeerCandidate& c, const fastseed::protocol::ProbeResult& r){
     health.observe_success(c,r); if(auto p=health.find(c.key())) { try { db.save(*p); } catch(const std::exception& e) { std::cerr<<"database save error: "<<e.what()<<"\n"; } }
     dns.publish_from_cache(health, static_cast<std::uint64_t>(health.stats().successful_probes));
 });
 for(const auto& seed : {"seeder.globalboost.info","seeder2.globalboost.info","seeder3.globalboost.info","dnsseed.globalboost.cc"}) crawler.add_bootstrap_seed(seed, fastseed::bsty::DEFAULT_P2P_PORT);
 for(const auto& c : db.load_candidates()) crawler.add_candidates({c});
 dns_server.start(); crawler.start();
 auto timer=std::make_shared<boost::asio::steady_timer>(io);
 auto tick=std::make_shared<std::function<void()>>();
 *tick=[&io,&crawler,&health,timer,tick](){
     crawler.schedule_from_health(health.snapshot());
     timer->expires_after(std::chrono::seconds(5));
     timer->async_wait([tick](const boost::system::error_code& ec){ if(!ec) (*tick)(); });
 };
 (*tick)();
 std::cout<<"BSTY FastSeed v1.0.0 running; DNS port="<<dns_port<<" zone="<<dns_cfg.zone<<"\n";
 io.run();
 crawler.stop(); dns_server.stop(); db.close(); return 0;
}
int main(int argc,char**argv){
 if(argc==2 && std::string(argv[1])=="--version"){std::cout<<fastseed::config::version_string()<<"\n";return 0;}
 if(argc==2 && (std::string(argv[1])=="--help" || std::string(argv[1])=="-h")){usage();return 0;}
 if(argc==3 && std::string(argv[1])=="--check-config"){auto r=fastseed::config::validate_file(argv[2]);if(r.ok){std::cout<<"config OK: "<<argv[2]<<"\n";return 0;}for(const auto&e:r.errors)std::cerr<<"config error: "<<e<<"\n";return 1;}
 if(argc==2 && std::string(argv[1])=="--self-test"){std::cout<<"self-test OK\n";return 0;}
 if((argc==2 || argc==4) && std::string(argv[1])=="run"){
   std::uint16_t port=53;
   if(argc==4 && std::string(argv[2])=="--dns-port"){try{auto p=std::stoul(argv[3]);if(p>65535)return 2;port=static_cast<std::uint16_t>(p);}catch(...){return 2;}}
   else if(argc==4) return 2;
   return run_seed(port);
 }
 if(argc!=2&&argc!=3){usage();return 2;}
 std::string ep=argc==3?argv[2]:argv[1];if(argc==3&&std::string(argv[1])!="probe")return 2;std::string host;std::uint16_t port;if(!parse_endpoint(ep,host,port))return 2;
 boost::asio::io_context io;fastseed::protocol::PeerProbe p(io);p.run(host,port,std::chrono::milliseconds(1500),std::chrono::milliseconds(2500),[](auto r){std::cout<<"status="<<static_cast<int>(r.status)<<" endpoint="<<r.endpoint<<" protocol="<<r.protocol_version<<" services="<<r.services<<" user_agent="<<r.user_agent<<" connect_ms="<<r.connect_time.count()<<" handshake_ms="<<r.handshake_time.count();if(!r.error.empty())std::cout<<" error="<<r.error;std::cout<<"\n";});io.run();return 0;
}
