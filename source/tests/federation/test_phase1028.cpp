#include "fastseed/federation/federation.hpp"
#include <cassert>
#include <iostream>
using namespace fastseed::federation;

static PeerRecord peer(const char* host, int port, long observed, const char* source) {
    return PeerRecord{host, static_cast<std::uint16_t>(port), 70016, 0, 5, 1, observed, observed-1, static_cast<std::uint64_t>(observed), source};
}

int main() {
    FederationConfig ca; ca.seeder_id="A"; ca.shared_secret="mesh-secret";
    FederationConfig cb=ca; cb.seeder_id="B";
    FederationConfig cc=ca; cc.seeder_id="C";
    SeederMesh a(ca), b(cb), c(cc);
    FederationMessage m;

    auto ab = a.make_sync(1, 1000, {peer("192.0.2.10",8226,1000,"A"), peer("192.0.2.11",8226,1000,"A")});
    assert(b.accept(ab,1001,m));
    FederatedPeerStore sb("B");
    assert(sb.apply(m)==2 && sb.size()==2);

    // Duplicate/replayed federation traffic is rejected by the sender/sequence guard.
    assert(!b.accept(ab,1001,m));

    // B forwards intelligence to C, but C's export excludes B-sourced records,
    // preventing simple rebroadcast loops.
    auto forwarded = b.make_sync(1,1002,sb.snapshot(256,"C"));
    assert(c.accept(forwarded,1003,m));
    FederatedPeerStore sc("C");
    assert(sc.apply(m)==2 && sc.size()==2);
    assert(sc.snapshot(256,"B").size()==2);
    // If C re-exports A-originated records, A ignores them locally; this
    // prevents an A->B->C->A intelligence loop from mutating A's state.
    auto c_to_a = c.make_sync(1,1004,sc.snapshot(256,"A"));
    assert(a.accept(c_to_a,1005,m));
    FederatedPeerStore sa("A");
    assert(sa.apply(m)==0 && sa.size()==0);

    // Newer observation wins regardless of arrival order.
    FederationMessage newer{MessageType::PeerSync,1,"A",2,1010,{peer("192.0.2.10",8226,1010,"A")},{}};
    assert(sb.apply(newer)==1);
    auto snap=sb.snapshot(); assert(snap.size()==2 && snap[0].observed_at_unix==1010);

    // Equal timestamps use deterministic source-seeder tie breaking.
    FederationMessage tie{MessageType::PeerSync,1,"C",1,1020,{peer("192.0.2.10",8226,1010,"Z")},{}};
    assert(sb.apply(tie)==1);
    snap=sb.snapshot(); assert(snap[0].source_seeder=="Z");

    // Withdrawal removes the federated record without affecting other peers.
    auto wd=b.make_withdraw(2,1030,{"192.0.2.11:8226"});
    assert(c.accept(wd,1031,m));
    assert(sc.apply(m)==1 && !sc.contains("192.0.2.11:8226") && sc.contains("192.0.2.10:8226"));

    // Authentication and malformed federation frames are rejected.
    auto bad=ab; bad.back()^=0x80; assert(!c.accept(bad,1032,m));
    auto truncated=ab; truncated.resize(12); assert(!c.accept(truncated,1032,m));

    // Independent stores remain independent; one seeder's withdrawal cannot
    // mutate another seeder's local state until a valid federation message arrives.
    assert(sb.contains("192.0.2.11:8226"));

    std::cout << "phase1028_federation PASS\n";
    return 0;
}
