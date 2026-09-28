#include "fastseed/crawler/adaptive_feedback.hpp"
#include "fastseed/db/peer_database.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

int main() {
    using namespace fastseed;
    const auto path = std::filesystem::temp_directory_path() / "fastseed-phase1014.sqlite3";
    std::error_code ec; std::filesystem::remove(path, ec); std::filesystem::remove(path.string()+".lock", ec);

    db::PeerDatabase db({path.string()});
    db.open();
    crawler::AdaptiveCapacityController ctl;
    for (int i=0;i<4;++i) ctl.record(transport::TransportKind::DirectTcp, crawler::ProbeOutcome::Success, std::chrono::milliseconds(10));
    auto state=ctl.snapshot(transport::TransportKind::DirectTcp);
    state.updated_at_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    db.save_scheduler_state(state, transport::TransportKind::DirectTcp);
    auto rows=db.load_scheduler_state(); assert(rows.size()==1); assert(rows[0].state.learned_capacity==state.learned_capacity);
    assert(db.scheduler_state_fresh(state.updated_at_ms, 60000));
    assert(!db.scheduler_state_fresh(state.updated_at_ms-120000, 60000));

    crawler::AdaptiveCapacityController restored;
    assert(restored.restore(transport::TransportKind::DirectTcp, rows[0].state));
    assert(restored.capacity(transport::TransportKind::DirectTcp)==state.learned_capacity);
    auto bad=state; bad.state_version=99; assert(!restored.restore(transport::TransportKind::DirectTcp,bad));
    auto stale=state; stale.updated_at_ms-=8LL*24*60*60*1000; assert(!restored.restore(transport::TransportKind::DirectTcp,stale));
    auto too_high=state; too_high.learned_capacity=999999; assert(!restored.restore(transport::TransportKind::DirectTcp,too_high));

    bool locked=false;
    try { db::PeerDatabase second({path.string()}); second.open(); } catch(const db::DatabaseError& e) { locked=(e.code()==db::DatabaseErrorCode::LockFailed); }
    assert(locked);
    db.close();
    { db::PeerDatabase second({path.string()}); second.open(); second.close(); }
    db.close();
    std::filesystem::remove(path, ec); std::filesystem::remove(path.string()+".lock", ec);
    std::cout << "phase1014 integrity tests: PASS\n";
}
