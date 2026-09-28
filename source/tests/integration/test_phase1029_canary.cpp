#include "fastseed/canary/mainnet_canary.hpp"
#include <cassert>
#include <iostream>
using namespace fastseed::canary;
using namespace fastseed::protocol;

static ProbeResult result(ProbeStatus s, int v=70016, std::uint64_t services=0) {
    ProbeResult r; r.status=s; r.protocol_version=v; r.services=services; return r;
}
int main() {
    CanaryPolicy p; p.max_targets=3;
    MainnetCanary c(p);
    std::string why;
    assert(c.validate_target("seed.example", 8226, why));
    assert(!c.validate_target("seed.example", 8333, why) && why=="non-mainnet port");
    assert(!c.validate_target(std::string(256,'x'),8226,why));

    auto ok=c.evaluate("seed.example",8226,result(ProbeStatus::Success));
    assert(ok.accepted);
    auto old=c.evaluate("old.example",8226,result(ProbeStatus::Success,31799));
    assert(!old.accepted && old.rejection_reason=="peer protocol below minimum");
    auto bad=c.evaluate("bad.example",8226,result(ProbeStatus::InvalidChecksum));
    assert(!bad.accepted);

    auto targets=c.bounded_targets({"a","b","a","c","d","e"});
    assert((targets==std::vector<std::string>{"a","b","c"}));

    // Service-zero peers remain valid: FastSeed does not invent service claims.
    auto zero=c.evaluate("zero.example",8226,result(ProbeStatus::Success,70016,0));
    assert(zero.accepted);

    // Canary policy does not expose or invoke blockchain synchronization.
    assert(p.required_port==fastseed::bsty::DEFAULT_P2P_PORT);
    assert(p.minimum_protocol==fastseed::bsty::MIN_PEER_VERSION);
    std::cout << "phase1029_mainnet_canary PASS\n";
}
