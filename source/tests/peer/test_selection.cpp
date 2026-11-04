#include "fastseed/health/peer_health.hpp"
#include "../protocol/test_support.hpp"
#include <algorithm>
using namespace fastseed;
int main() {
    test_support::Suite t;const auto now=std::chrono::system_clock::now();health::PeerHealthCache cache;
    auto add=[&](const char* host,health::PeerState state,int age,std::uint64_t services) {
        health::PeerHealth p;p.host=host;p.port=8226;p.state=state;p.last_success=now-std::chrono::seconds(age);p.last_seen=p.last_success;p.services=services;p.successful_probes=3;cache.restore(p);
    };
    add("192.0.2.1",health::PeerState::Hot,3,9);add("192.0.3.1",health::PeerState::Hot,2,1);
    add("198.51.100.1",health::PeerState::Warm,1,9);add("2001:db8::1",health::PeerState::Hot,4,9);
    add("203.0.113.1",health::PeerState::Hot,2000,9);add("203.0.113.2",health::PeerState::Hot,-10,9);
    add("203.0.113.3",health::PeerState::Quarantined,1,9);
    health::SelectionPolicy policy;policy.max_results=10;policy.required_services=9;auto selected=cache.select_snapshot(policy,now);
    t.check(selected.size()==3 && selected[0].host=="192.0.2.1" && selected[1].host=="2001:db8::1" && selected[2].state==health::PeerState::Warm,"state/freshness/services ordering");
    policy.include_warm=false;t.check(cache.select_snapshot(policy,now).size()==2,"warm exclusion");
    policy.required_services=0;policy.ipv6=false;policy.max_per_ip_group=1;selected=cache.select_snapshot(policy,now);
    t.check(selected.size()==1 && selected[0].host=="192.0.3.1","optional IPv4 /16 diversity cap");
    policy.max_results=0;t.check(cache.select_snapshot(policy,now).empty(),"zero result bound");
    policy.max_results=1;policy.max_per_ip_group=0;t.check(cache.select_snapshot(policy,now).size()==1,"result bound");
    t.check(cache.dns_snapshot(20,true,now).size()==4,"default DNS selection preserves eligible IP peers");
    auto first=cache.select_snapshot(policy,now),second=cache.select_snapshot(policy,now);t.check(first[0].key()==second[0].key(),"stable repeated selection");
    health::PeerHealth tor;tor.host="onion:"+std::string(64,'2');tor.port=8226;tor.network=crawler::CandidateNetwork::TorV3;tor.address_bytes.assign(32,0x22);tor.state=health::PeerState::Hot;tor.last_success=now;tor.successful_probes=3;cache.restore(tor);
    policy.ipv4=false;policy.ipv6=false;policy.tor_v3=true;policy.max_results=10;selected=cache.select_snapshot(policy,now);
    t.check(selected.size()==1 && selected[0].network==crawler::CandidateNetwork::TorV3,"explicit overlay selection");
    t.check(cache.dns_snapshot(20,true,now).size()==4,"overlay filter stays outside DNS");
    auto i2p=tor;i2p.host="i2p:"+std::string(64,'2');i2p.network=crawler::CandidateNetwork::I2P;cache.restore(i2p);
    policy.tor_v3=false;policy.i2p=true;selected=cache.select_snapshot(policy,now);
    t.check(selected.size()==1 && selected[0].network==crawler::CandidateNetwork::I2P,"explicit I2P selection");
    health::PeerHealthCache v6;
    for(const char* host:{"2001:db8::1","2001:db8:1::1","2001:db9::1"}) {
        health::PeerHealth p;p.host=host;p.port=8226;p.state=health::PeerState::Hot;p.last_success=now;v6.restore(p);
    }
    policy.i2p=false;policy.ipv6=true;policy.max_per_ip_group=1;
    t.check(v6.select_snapshot(policy,now).size()==2,"optional IPv6 /32 diversity cap");
    health::PeerHealthCache reverse;auto records=cache.snapshot();std::reverse(records.begin(),records.end());reverse.restore(records);
    policy={};policy.max_results=20;policy.tor_v3=policy.i2p=true;
    first=cache.select_snapshot(policy,now);second=reverse.select_snapshot(policy,now);
    t.check(first.size()==second.size(),"selection size independent of insertion order");
    for(std::size_t i=0;i<first.size();++i)t.check(first[i].key()==second[i].key(),"selection ordering independent of insertion order");
    t.pass("Alpha3 peer selection");
}
