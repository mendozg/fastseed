#include "fastseed/dns/dns_engine.hpp"
#include "fastseed/dns/rate_limiter.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace fastseed;

static std::vector<std::uint8_t> query(std::uint16_t id, std::string name, std::uint16_t type) {
    std::vector<std::uint8_t> q{std::uint8_t(id>>8),std::uint8_t(id),0x01,0x00,0,1,0,0,0,0,0,0};
    std::size_t start=0;
    while(start<name.size()){ auto dot=name.find('.',start); auto end=dot==std::string::npos?name.size():dot; q.push_back(end-start); q.insert(q.end(),name.begin()+start,name.begin()+end); if(dot==std::string::npos) break; start=dot+1; }
    q.push_back(0); q.push_back(type>>8); q.push_back(type); q.push_back(0); q.push_back(1); return q;
}

int main() {
    dns::RateLimiter::Config rc; rc.per_source_per_second=2; rc.global_per_second=3; rc.max_sources=2;
    dns::RateLimiter limiter(rc);
    const auto t = std::chrono::steady_clock::now();
    assert(limiter.allow("192.0.2.1", t));
    assert(limiter.allow("192.0.2.1", t));
    assert(!limiter.allow("192.0.2.1", t));
    assert(limiter.allow("192.0.2.2", t));
    assert(!limiter.allow("192.0.2.3", t));
    assert(limiter.allow("192.0.2.1", t + std::chrono::seconds(1)));

    dns::DnsConfig cfg; cfg.zone="seed.globalboost.info"; cfg.max_answers=64; cfg.ttl_seconds=30; cfg.max_response_size=512;
    dns::DnsResponseEngine engine(cfg);
    health::PeerHealthCache cache;
    protocol::ProbeResult ok{}; ok.status=protocol::ProbeStatus::Success; ok.protocol_version=70016; ok.user_agent="/BSTYFastSeed/";
    for (int i=0; i<64; ++i) {
        crawler::PeerCandidate c{"192.0.2." + std::to_string(i + 1),8226,crawler::CandidateSource::Manual};
        cache.observe_success(c,ok); cache.observe_success(c,ok); cache.observe_success(c,ok);
    }
    engine.publish_from_cache(cache,42);
    auto q=query(0x1234,"seed.globalboost.info",dns::TYPE_A);
    auto response=engine.build_response(q);
    assert(response.size() <= 512);
    assert((response[2] & 0x80) != 0);
    assert((response[2] & 0x02) != 0 || response[7] > 0);
    std::cout << "Phase 6 tests PASS\n";
}
