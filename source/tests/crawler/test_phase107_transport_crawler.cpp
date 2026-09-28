#include "fastseed/crawler/crawler.hpp"
#include "fastseed/protocol/constants.hpp"
#include <cassert>
#include <iostream>

using namespace fastseed;

class CandidateAwareRunner final : public crawler::ProbeRunner {
public:
    void run(const std::string&, std::uint16_t, std::chrono::milliseconds,
             std::chrono::milliseconds, std::function<void(protocol::ProbeResult)> done) override {
        ++legacy_calls;
        protocol::ProbeResult r; r.status=protocol::ProbeStatus::Success; done(std::move(r));
    }

    void run_candidate(const crawler::PeerCandidate& candidate,
                       std::chrono::milliseconds,
                       std::chrono::milliseconds,
                       std::function<void(protocol::ProbeResult)> done) override {
        ++candidate_calls;
        seen_networks.push_back(candidate.network);
        protocol::ProbeResult r; r.status=protocol::ProbeStatus::Success;
        r.endpoint=candidate.key(); r.protocol_version=bsty::PROTOCOL_VERSION;
        done(std::move(r));
    }

    int legacy_calls{0};
    int candidate_calls{0};
    std::vector<crawler::CandidateNetwork> seen_networks;
};

int main() {
    boost::asio::io_context io;
    auto runner=std::make_unique<CandidateAwareRunner>();
    auto* raw=runner.get();
    crawler::CrawlerConfig cfg;
    cfg.max_concurrent_probes=8;
    crawler::PeerCrawler crawler(io,cfg,std::move(runner));

    crawler::PeerCandidate ipv4; ipv4.host="127.0.0.1"; ipv4.port=8226; ipv4.network=crawler::CandidateNetwork::IPv4;
    crawler::PeerCandidate tor; tor.host="onion:aaaaaaaa"; tor.port=8226; tor.network=crawler::CandidateNetwork::TorV3; tor.address_bytes.resize(32,0x11);
    crawler::PeerCandidate i2p; i2p.host="i2p:bbbbbbbb"; i2p.port=8226; i2p.network=crawler::CandidateNetwork::I2P; i2p.address_bytes.resize(32,0x22);

    crawler.add_candidates({ipv4,tor,i2p});
    crawler.start();

    auto st=crawler.stats();
    assert(raw->candidate_calls==3);
    assert(raw->legacy_calls==0);
    assert(st.successful_probes==3);
    assert(st.validated==3);
    assert(st.torv3_candidates==1);
    assert(st.i2p_candidates==1);

    auto snap=crawler.snapshot();
    assert(snap.size()==3);
    for(const auto& c:snap) assert(c.state==crawler::CandidateState::Validated);

    std::cout << "phase107_transport_crawler PASS\n";
}
