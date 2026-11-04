#include "fastseed/health/peer_health.hpp"
#include "fastseed/crawler/crawler.hpp"
#include "../protocol/test_support.hpp"
#include <limits>
using namespace fastseed;
crawler::PeerCandidate candidate(std::string host){crawler::PeerCandidate c;c.host=std::move(host);c.port=8226;return c;}
int main() {
    test_support::Suite t;health::HealthConfig cfg;cfg.minimum_observations_for_hot=2;cfg.quarantine_after_failures=2;
    health::PeerHealthCache cache(cfg);auto first=candidate("2001:db8::1"),alias=candidate("2001:0DB8:0:0:0:0:0:1");
    protocol::ProbeResult ok;ok.status=protocol::ProbeStatus::Success;ok.protocol_version=70016;ok.handshake_time=std::chrono::milliseconds(8);
    protocol::ProbeResult bad;bad.status=protocol::ProbeStatus::Timeout;bad.error="timeout";
    cache.observe_success(first,ok);cache.observe_success(alias,ok);
    t.check(cache.stats().total==1 && cache.find(alias.key())->successful_probes==2,"aliases share probe history");
    t.check(cache.find(first.endpoint()->id())->state==health::PeerState::Hot,"typed ID lookup");
    auto original=*cache.find(first.key());alias.source=crawler::CandidateSource::PeerExchange;cache.observe_discovered(alias);
    t.check(cache.find(first.key())->state==health::PeerState::Hot && cache.find(first.key())->last_success==original.last_success && cache.find(first.key())->source==original.source,"rediscovery preserves local health");
    cache.observe_failure(alias,bad);cache.observe_failure(first,bad);
    const auto until=cache.find(first.key())->quarantine_until;cache.observe_discovered(alias);
    t.check(cache.find(first.key())->state==health::PeerState::Quarantined && cache.find(first.key())->quarantine_until==until && cache.find(first.key())->last_error=="timeout","rediscovery cannot clear quarantine");
    cache.restore(original);t.check(cache.find(first.key())->state==health::PeerState::Quarantined,"restore cannot overwrite a current local observation");
    t.check(cache.dns_snapshot(10).empty(),"quarantine excluded from DNS");
    cache.observe_success(first,bad);cache.observe_failure(first,ok);t.check(cache.find(first.key())->failed_probes==2,"wrong result kinds ignored");
    cache.observe_success(candidate("seed.example"),ok);t.check(cache.stats().total==1,"unresolved names are not endpoint identities");
    health::PeerHealthCache restored(cfg);original.host=alias.host;restored.restore(original);
    auto older=original;older.host=first.host;older.last_success-=std::chrono::hours(1);older.successful_probes=99;restored.restore(older);
    t.check(restored.stats().total==1 && restored.find(first.key())->successful_probes==2,"restore aliases choose newest evidence without double counting");
    health::PeerHealthCache reverse(cfg);reverse.restore(older);reverse.restore(original);
    t.check(reverse.find(first.key())->last_success==restored.find(first.key())->last_success,"restore outcome independent of input order");
    auto mapped=candidate("::ffff:192.0.2.1"),ipv4=candidate("192.0.2.1");restored.observe_success(mapped,ok);restored.observe_success(ipv4,ok);
    t.check(restored.find(ipv4.key())->network==crawler::CandidateNetwork::IPv4 && restored.find(ipv4.key())->successful_probes==2,"mapped IPv6 uses IPv4 identity and state");
    auto overlay=candidate("onion:"+std::string(64,'1'));overlay.network=crawler::CandidateNetwork::TorV3;overlay.address_bytes.assign(32,0x11);
    restored.observe_success(overlay,ok);const auto record=*restored.find(overlay.key());health::PeerHealthCache overlay_restore;overlay_restore.restore(record);
    t.check(overlay_restore.contains(overlay.key()) && overlay_restore.contains(record.host+":8226"),"overlay discovery and legacy restore keys agree");
    t.check(overlay_restore.dns_snapshot(10).empty(),"overlay never becomes A/AAAA");
    cfg.max_peers=3;cfg.hot_ipv4=1;cfg.hot_ipv6=1;health::PeerHealthCache bounded(cfg);bounded.observe_success(ipv4,ok);bounded.observe_success(ipv4,ok);
    for(unsigned i=1;i<=20;++i)bounded.observe_discovered(candidate("198.51.100."+std::to_string(i)));
    t.check(bounded.stats().total==3 && bounded.contains(ipv4.key()),"all-state bound preserves validated peer under discovery flood");
    bounded.observe_success(first,ok);bounded.observe_success(first,ok);
    t.check(bounded.stats().hot_ipv4==1 && bounded.stats().hot_ipv6==1 && bounded.stats().total<=3,"hot family quotas and total bound");
    auto full=record;full.observations=full.successful_probes=full.failed_probes=std::numeric_limits<std::uint64_t>::max();
    health::PeerHealthCache counters;counters.restore(full);counters.observe_success(overlay,ok);
    t.check(counters.find(overlay.key())->observations==UINT64_MAX && counters.find(overlay.key())->successful_probes==UINT64_MAX,"counter increments saturate");
    t.check(counters.find(overlay.key())->availability()==0.5,"availability sum cannot overflow");
    t.check(counters.stats().observations==UINT64_MAX,"aggregate counters saturate");
    cfg.max_peers=2;health::PeerHealthCache held(cfg);held.observe_failure(ipv4,bad);held.observe_failure(ipv4,bad);
    for(unsigned i=1;i<12;++i)held.observe_discovered(candidate("198.51.100."+std::to_string(i)));
    t.check(held.stats().total==2 && held.find(ipv4.key())->state==health::PeerState::Quarantined,"candidate flood retains quarantine");
    cfg.latency_history_size=2;cfg.cooldown_multiplier=1.0;health::PeerHealthCache extreme(cfg);
    full.latency_history={std::chrono::milliseconds(-1),std::chrono::milliseconds::max(),std::chrono::milliseconds::max()};
    full.consecutive_failures=UINT32_MAX;extreme.restore(full);extreme.observe_failure(overlay,bad);
    t.check(extreme.find(overlay.key())->consecutive_failures==UINT32_MAX && extreme.find(overlay.key())->latency_history.size()==2 && extreme.find(overlay.key())->latency_avg==std::chrono::milliseconds::max(),"bounded history and saturated failure cooldown");
    auto tied=original;tied.last_failure=tied.last_success;tied.state=health::PeerState::Quarantined;tied.quarantine_until=std::chrono::system_clock::now()+std::chrono::hours(1);
    health::PeerHealthCache ties;ties.restore(original);ties.restore(tied);ties.restore(original);
    t.check(ties.find(first.key())->state==health::PeerState::Quarantined,"tied failure evidence wins over restored success");
    boost::asio::io_context io;crawler::PeerCrawler crawler(io);
    t.check(crawler.add_candidate(first.host,first.port,first.source) && !crawler.add_candidate(alias.host,alias.port,alias.source),"crawler deduplicates IPv6 aliases");
    t.check(crawler.add_candidate(mapped.host,mapped.port,mapped.source) && !crawler.add_candidate(ipv4.host,ipv4.port,ipv4.source),"crawler deduplicates mapped IPv4 aliases");
    t.check(crawler.stats().candidates_total==2 && crawler.stats().ipv4_candidates==1 && crawler.stats().ipv6_candidates==1,"crawler canonical family counts");
    counters.clear();t.check(counters.stats().total==0 && !counters.find(record.id().value()),"clear removes identities and state");
    t.pass("Alpha3 peer health cache");
}
