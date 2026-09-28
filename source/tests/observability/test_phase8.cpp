#include "fastseed/metrics/metrics.hpp"
#include "fastseed/metrics/logger.hpp"
#include "fastseed/dns/rate_limiter.hpp"
#include "fastseed/federation/federation.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
using namespace fastseed;
int main(){
 metrics::Registry r; auto& c=r.counter("fastseed_probe_success_total"); auto& g=r.gauge("fastseed_active_probes");
 constexpr int threads=32, loops=1000; std::vector<std::thread> ts; for(int i=0;i<threads;++i) ts.emplace_back([&]{for(int j=0;j<loops;++j)c.inc();}); for(auto& t:ts)t.join(); assert(c.value()==threads*loops); g.set(256);g.dec(1);assert(g.value()==255); auto text=r.prometheus();assert(text.find("fastseed_probe_success_total ")!=std::string::npos);
 dns::RateLimiter::Config lc;lc.per_source_per_second=100000;lc.global_per_second=1000000;lc.max_sources=1024;dns::RateLimiter lim(lc); for(int i=0;i<1000;++i) assert(lim.allow("192.0.2.1"));
 federation::FederationConfig fc;fc.seeder_id="a";fc.shared_secret="secret";federation::SeederMesh a(fc);fc.seeder_id="b";federation::SeederMesh b(fc); federation::PeerRecord p{"192.0.2.10",8226,70016,0,1,0,100,0,100,"a"}; std::vector<std::thread> fs; std::atomic<int> ok{0}; for(int i=0;i<16;++i) fs.emplace_back([&,i]{auto w=a.make_sync((std::uint64_t)i+1,1000+i,{p}); federation::FederationMessage m;if(b.accept(w,1000+i,m))ok++;});for(auto& t:fs)t.join();assert(ok==16);
 metrics::Logger logger(metrics::LogLevel::Error); logger.log(metrics::LogLevel::Debug,"stress complete"); std::cout<<"phase8_tests PASS\n"; }
