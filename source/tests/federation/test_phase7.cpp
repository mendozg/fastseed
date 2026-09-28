#include "fastseed/federation/federation.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace fastseed::federation;
int main(){
 FederationConfig c; c.seeder_id="seeder-a"; c.shared_secret="test-secret"; c.replay_window=std::chrono::seconds(60);
 SeederMesh a(c); FederationConfig d=c; d.seeder_id="seeder-b"; SeederMesh b(d);
 auto h=a.make_hello(1,1000); FederationMessage m; assert(b.accept(h,1001,m)); assert(m.type==MessageType::Hello); assert(m.sender_id=="seeder-a");
 assert(!b.accept(h,1001,m));
 PeerRecord p{"192.0.2.10",8226,70016,0,8,1,900,800,1000,"seeder-a"};
 auto s=a.make_sync(2,1002,{p}); assert(b.accept(s,1003,m)); assert(m.peers.size()==1); assert(m.peers[0].key()=="192.0.2.10:8226");
 auto bad=s; bad.back()^=1; assert(!b.accept(bad,1003,m));
 auto old=a.make_hello(3,900); assert(!b.accept(old,1003,m));
 auto w=a.make_withdraw(4,1004,{p.key()}); assert(b.accept(w,1005,m)); assert(m.withdrawals.size()==1);
 auto self=a.make_hello(1,1000); SeederMesh selfmesh(c); assert(!selfmesh.accept(self,1000,m));
 std::cout<<"phase7_tests PASS\n"; return 0;
}
