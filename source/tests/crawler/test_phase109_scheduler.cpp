#include "fastseed/crawler/scheduler.hpp"
#include <cassert>
#include <chrono>
#include <vector>
using namespace fastseed;
int main() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    health::PeerHealth hot; hot.host="1.2.3.4"; hot.port=8226; hot.network=crawler::CandidateNetwork::IPv4; hot.state=health::PeerState::Hot; hot.next_probe=now-seconds(1); hot.successful_probes=10;
    health::PeerHealth tor; tor.host="onion:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; tor.port=8226; tor.network=crawler::CandidateNetwork::TorV3; tor.state=health::PeerState::Hot; tor.next_probe=now-seconds(2);
    health::PeerHealth future; future.host="5.6.7.8"; future.port=8226; future.network=crawler::CandidateNetwork::IPv4; future.state=health::PeerState::Hot; future.next_probe=now+minutes(5);
    health::PeerHealth quarantined; quarantined.host="9.9.9.9"; quarantined.port=8226; quarantined.network=crawler::CandidateNetwork::IPv4; quarantined.state=health::PeerState::Quarantined; quarantined.quarantine_until=now+minutes(1); quarantined.next_probe=now-seconds(1);
    crawler::HealthAwareScheduler s;
    auto out=s.select_due({hot,tor,future,quarantined},now);
    assert(out.size()==2);
    assert(out[0].candidate.network==crawler::CandidateNetwork::IPv4);
    assert(out[1].candidate.network==crawler::CandidateNetwork::TorV3);
    assert(out[1].candidate.state==crawler::CandidateState::AwaitingTransport);
    assert(out[0].candidate.source==crawler::CandidateSource::PersistentDatabase);
    return 0;
}
