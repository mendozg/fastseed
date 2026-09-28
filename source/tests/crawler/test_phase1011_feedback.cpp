#include "fastseed/crawler/adaptive_feedback.hpp"
#include <cassert>
using namespace fastseed;
int main() {
    crawler::FeedbackConfig cfg;
    cfg.global_max_inflight=256; cfg.direct_tcp={4,8,16}; cfg.tor_socks5={2,4,8}; cfg.i2p_sam={2,4,8}; cfg.adjustment_step=2; cfg.cooldown_after_adjustment=std::chrono::seconds(0);
    crawler::AdaptiveCapacityController c(cfg);
    assert(c.capacity(transport::TransportKind::DirectTcp)==8);
    for(int i=0;i<4;i++) c.record(transport::TransportKind::DirectTcp,crawler::ProbeOutcome::Success,std::chrono::milliseconds(100));
    assert(c.capacity(transport::TransportKind::DirectTcp)==10);
    for(int i=0;i<4;i++) c.record(transport::TransportKind::TorSocks5,crawler::ProbeOutcome::Timeout,std::chrono::milliseconds(2000));
    assert(c.capacity(transport::TransportKind::TorSocks5)==2);
    assert(c.admit(transport::TransportKind::I2pSam,3));
    assert(!c.admit(transport::TransportKind::I2pSam,4));
    assert(c.total_capacity()<=256);
    c.reset(transport::TransportKind::DirectTcp); assert(c.capacity(transport::TransportKind::DirectTcp)==8);
    return 0;
}
