#include "fastseed/db/peer_database.hpp"
#include "fastseed/db/persistence_queue.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace fastseed;

static health::PeerHealth make_peer(const std::string& host, std::uint16_t port, health::PeerState state) {
    health::PeerHealth p;
    p.host=host; p.port=port; p.state=state; p.source=crawler::CandidateSource::AddrV2;
    const auto now=std::chrono::system_clock::now(); p.first_seen=now; p.last_seen=now; p.last_success=now;
    p.protocol_version=70016; p.services=1; p.user_agent="/BSTYTest:4.0/";
    p.observations=3; p.successful_probes=3; p.consecutive_successes=3;
    p.latency_last=std::chrono::milliseconds(20); p.latency_avg=std::chrono::milliseconds(25); p.latency_p95=std::chrono::milliseconds(30);
    p.latency_history={std::chrono::milliseconds(20),std::chrono::milliseconds(25),std::chrono::milliseconds(30)};
    return p;
}

int main() {
    const auto path=std::filesystem::temp_directory_path()/"bsty_fastseed_phase4_test.sqlite3";
    std::error_code ec; std::filesystem::remove(path,ec); std::filesystem::remove(path.string()+"-wal",ec); std::filesystem::remove(path.string()+"-shm",ec);
    db::PeerDatabase db({path.string(),250000,std::chrono::hours(24*30),std::chrono::hours(24*14),2500});
    db.open();
    assert(db.integrity_check());
    auto p=make_peer("192.0.2.10",8226,health::PeerState::Hot);
    db.save(p);
    auto p6=make_peer("2001:db8::10",8226,health::PeerState::Warm); db.save(p6);
    auto loaded=db.load_health(); assert(loaded.size()==2);
    auto candidates=db.load_candidates(); assert(candidates.size()==2);
    for(const auto& c:candidates) assert(c.state==crawler::CandidateState::Queued && c.source==crawler::CandidateSource::PersistentDatabase);
    auto s=db.stats(); assert(s.peers==2 && s.hot==1 && s.warm==1);
    db.save_batch({make_peer("198.51.100.2",8226,health::PeerState::Cold), make_peer("198.51.100.3",8226,health::PeerState::Quarantined)});
    assert(db.stats().peers==4);
    db::PersistenceQueue q(db,{16,2,std::chrono::milliseconds(20)});
    assert(q.enqueue(make_peer("198.51.100.4",8226,health::PeerState::Warm)));
    assert(q.enqueue(make_peer("198.51.100.5",8226,health::PeerState::Warm)));
    q.flush(); assert(q.pending()==0); assert(q.committed()>=2); q.stop();
    assert(db.stats().peers==6);
    assert(db.migrate()==0);
    db.close();
    std::filesystem::remove(path,ec); std::filesystem::remove(path.string()+"-wal",ec); std::filesystem::remove(path.string()+"-shm",ec);
    std::cout << "phase4 tests passed\n";
}
