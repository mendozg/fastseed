#include "fastseed/config/runtime_config.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/dns/dns_engine.hpp"
#include "fastseed/canary/mainnet_canary.hpp"
#include <cassert>
#include <iostream>
int main() {
    assert(fastseed::config::version_string() == "BSTY-FastSeed 1.0.0 (Production)");
    assert(fastseed::bsty::DEFAULT_P2P_PORT == 8226);
    assert(fastseed::bsty::PROTOCOL_VERSION == 70016);
    assert(fastseed::bsty::MIN_PEER_VERSION == 31800);
    fastseed::dns::DnsConfig dns;
    assert(dns.listen_port == 53 && dns.max_response_size == 1232);
    fastseed::canary::MainnetCanary canary;
    std::string reason;
    assert(canary.validate_target("127.0.0.1", 8226, reason));
    assert(!canary.validate_target("127.0.0.1", 8333, reason));
    assert(!reason.empty());
    std::cout << "phase13 production release gate: PASS\n";
}
