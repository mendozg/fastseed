#include "fastseed/crawler/candidate.hpp"
#include "fastseed/crawler/crawler.hpp"
#include "fastseed/protocol/wire_messages.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace fastseed;
    protocol::AddrV2Address tor{1700000000u, 0, 0x04, std::vector<std::uint8_t>(32, 0x11), 8226};
    auto tc = crawler::make_addrv2_candidate(tor);
    assert(tc.network == crawler::CandidateNetwork::TorV3);
    assert(tc.state == crawler::CandidateState::AwaitingTransport);
    assert(!tc.transport_probe_supported());
    assert(tc.host.rfind("onion:", 0) == 0);

    protocol::AddrV2Address i2p{1700000000u, 0, 0x05, std::vector<std::uint8_t>(32, 0x22), 8226};
    auto ic = crawler::make_addrv2_candidate(i2p);
    assert(ic.network == crawler::CandidateNetwork::I2P);
    assert(ic.state == crawler::CandidateState::AwaitingTransport);

    protocol::AddrV2Address ip4{1700000000u, 0, 0x01, {203,0,113,7}, 8226};
    auto vc = crawler::make_addrv2_candidate(ip4);
    assert(vc.network == crawler::CandidateNetwork::IPv4);
    assert(vc.state == crawler::CandidateState::Queued);
    assert(vc.transport_probe_supported());

    boost::asio::io_context io;
    crawler::PeerCrawler c(io);
    assert(c.add_addrv2_candidate(tor));
    assert(c.add_addrv2_candidate(i2p));
    assert(c.add_addrv2_candidate(ip4));
    const auto s = c.stats();
    assert(s.torv3_candidates == 1);
    assert(s.i2p_candidates == 1);
    assert(s.ipv4_candidates == 1);
    std::cout << "phase10.3 candidate integration tests: PASS\n";
}
