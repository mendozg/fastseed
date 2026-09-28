#include "fastseed/dns/dns_engine.hpp"
#include <cassert>
#include <iostream>
#include <memory>

using namespace fastseed;

static std::vector<std::uint8_t> query(std::uint16_t id, std::string name, std::uint16_t type) {
    std::vector<std::uint8_t> q{std::uint8_t(id>>8),std::uint8_t(id),0x01,0x00,0,1,0,0,0,0,0,0};
    std::size_t start=0;
    while(start<name.size()){ auto dot=name.find('.',start); auto end=dot==std::string::npos?name.size():dot; q.push_back(end-start); q.insert(q.end(),name.begin()+start,name.begin()+end); if(dot==std::string::npos) break; start=dot+1; }
    q.push_back(0); q.push_back(type>>8); q.push_back(type); q.push_back(0); q.push_back(1); return q;
}

int main(){
    dns::DnsConfig cfg; cfg.zone="seed.globalboost.info"; cfg.max_answers=8; cfg.ttl_seconds=30;
    dns::DnsResponseEngine engine(cfg);
    health::PeerHealthCache cache;
    crawler::PeerCandidate c1{"192.0.2.10",8226,crawler::CandidateSource::Manual};
    protocol::ProbeResult ok{}; ok.status=protocol::ProbeStatus::Success; ok.protocol_version=70016; ok.services=0; ok.user_agent="/BSTYFastSeed/"; ok.connect_time=std::chrono::milliseconds(5); ok.handshake_time=std::chrono::milliseconds(8);
    cache.observe_success(c1,ok); cache.observe_success(c1,ok); cache.observe_success(c1,ok);
    crawler::PeerCandidate c2{"2001:db8::10",8226,crawler::CandidateSource::Manual}; cache.observe_success(c2,ok); cache.observe_success(c2,ok); cache.observe_success(c2,ok);
    engine.publish_from_cache(cache,7);
    auto snap=engine.snapshot(); assert(snap && snap->generation==7 && snap->peers.size()==2);
    auto qa=query(0x1234,"seed.globalboost.info",dns::TYPE_A); auto ra=engine.build_response(qa); assert(ra.size()>=12); assert(ra[0]==0x12&&ra[1]==0x34); assert((ra[2]&0x80)!=0); assert((ra[3]&0x04)==0); assert(ra[7]==1);
    auto qaaaa=query(0x5678,"seed.globalboost.info",dns::TYPE_AAAA); auto r6=engine.build_response(qaaaa); assert(r6.size()>=12 && r6[7]==1);
    auto qbad=query(0x1111,"other.example",dns::TYPE_A); auto rb=engine.build_response(qbad); assert((rb[3]&0x05)==0x05);
    auto qtxt=query(0x2222,"seed.globalboost.info",16); auto rt=engine.build_response(qtxt); assert(rt[7]==0);
    dns::SnapshotStore store; store.publish(std::make_shared<dns::DnsSnapshot>(*snap)); assert(store.load()->generation==7);
    std::cout<<"Phase 5 tests PASS\n";
}
