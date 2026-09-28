#include "fastseed/intelligence/network_intelligence.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace fastseed;
int main(){
    using namespace std::chrono;
    intelligence::NetworkIntelligence ni;
    health::PeerHealth a,b;
    a.network=crawler::CandidateNetwork::IPv4; a.address_bytes={10,1,2,3};
    a.successful_probes=9; a.failed_probes=1; a.latency_avg=milliseconds(100);
    auto now=system_clock::now(); a.last_success=now;
    b.network=crawler::CandidateNetwork::IPv4; b.address_bytes={10,1,2,4};
    b.successful_probes=1; b.failed_probes=9; b.latency_avg=milliseconds(2000); b.last_success=now-hours(12);
    auto pop=std::vector<health::PeerHealth>{a,b};
    auto sa=ni.score(a,pop,now); auto sb=ni.score(b,pop,now);
    assert(sa.total>=0 && sa.total<=1); assert(sb.total>=0 && sb.total<=1);
    assert(sa.total>sb.total);
    auto stale=ni.freshness(b,now); assert(stale<1.0 && stale>=0.0);
    std::cout << "phase12 network intelligence tests: PASS\n";
}
