#include "fastseed/crawler/crawler.hpp"
#include <cassert>
#include <memory>
using namespace fastseed;

class ImmediateRunner final : public crawler::ProbeRunner {
public:
    int calls{};
    void run(const std::string&, std::uint16_t, std::chrono::milliseconds,
             std::chrono::milliseconds, std::function<void(protocol::ProbeResult)> done) override {
        protocol::ProbeResult r;
        r.status = protocol::ProbeStatus::Success;
        r.connect_time = std::chrono::milliseconds(50);
        r.handshake_time = std::chrono::milliseconds(50);
        ++calls;
        done(std::move(r));
    }
};

int main() {
    boost::asio::io_context io;
    auto runner = std::make_unique<ImmediateRunner>();
    auto* raw = runner.get();
    crawler::CrawlerConfig cfg;
    cfg.feedback.direct_tcp.maximum = 256; // Leave room for the increase under test.
    crawler::PeerCrawler c(io, cfg, std::move(runner));
    const auto before = c.transport_capacity(transport::TransportKind::DirectTcp);
    c.add_candidate("127.0.0.1", 8226, crawler::CandidateSource::Manual);
    c.add_candidate("127.0.0.2", 8226, crawler::CandidateSource::Manual);
    c.add_candidate("127.0.0.3", 8226, crawler::CandidateSource::Manual);
    c.add_candidate("127.0.0.4", 8226, crawler::CandidateSource::Manual);
    c.start();
    io.run();
    assert(raw->calls == 4);
    const auto after = c.transport_capacity(transport::TransportKind::DirectTcp);
    assert(after > before);
    const auto st = c.stats();
    assert(st.successful_probes == 4);
    assert(st.failed == 0);
    return 0;
}
