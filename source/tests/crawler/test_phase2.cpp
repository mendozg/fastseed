#include "fastseed/crawler/crawler.hpp"
#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace fastseed::crawler;
using fastseed::protocol::ProbeResult;

class FakeRunner final : public ProbeRunner {
public:
    int active=0, max_active=0, calls=0;
    std::vector<std::function<void(ProbeResult)>> pending;
    void run(const std::string&, std::uint16_t, std::chrono::milliseconds,
             std::chrono::milliseconds, std::function<void(ProbeResult)> done) override {
        ++active; ++calls; if(active>max_active) max_active=active; pending.push_back(std::move(done));
    }
    void complete(bool success) {
        if (pending.empty()) throw std::runtime_error("no pending probe");
        auto cb=std::move(pending.front()); pending.erase(pending.begin()); --active;
        ProbeResult r; r.status=success ? fastseed::protocol::ProbeStatus::Success : fastseed::protocol::ProbeStatus::ConnectionFailed;
        if (!success) r.error="test"; cb(std::move(r));
    }
};

static int require(bool v, const char* what) { if(!v){std::cerr<<"FAIL: "<<what<<"\n";return 1;} return 0; }

int main(){
    boost::asio::io_context io;
    CrawlerConfig cfg; cfg.max_concurrent_probes=2; cfg.max_open_connections=2; cfg.max_candidates=10;
    auto fake=std::make_unique<FakeRunner>(); auto* fp=fake.get();
    PeerCrawler c(io,cfg,std::move(fake));
    int validated=0; c.set_validated_callback([&](const PeerCandidate&,const ProbeResult&){++validated;});
    if(require(c.add_candidate("127.0.0.1",8226,CandidateSource::Manual),"add IPv4 #1")) return 1;
    if(require(c.add_candidate("127.0.0.2",8226,CandidateSource::Addr),"add IPv4 #2")) return 1;
    if(require(c.add_candidate("2001:db8::1",8226,CandidateSource::AddrV2),"add IPv6")) return 1;
    if(require(!c.add_candidate("127.0.0.1",8226,CandidateSource::Manual),"deduplicate")) return 1;
    c.start();
    if(require(fp->calls==2 && fp->max_active==2,"concurrency limit")) return 1;
    fp->complete(true);
    if(require(fp->calls==3,"queue pumps after completion")) return 1;
    fp->complete(false);
    if(require(validated==1,"validated callback")) return 1;
    fp->complete(true);
    auto s=c.stats();
    if(require(s.candidates_total==3,"candidate count")) return 1;
    if(require(s.successful_probes==2 && s.failed==1,"probe outcomes")) return 1;
    if(require(s.ipv4_candidates==2 && s.ipv6_candidates==1,"address families")) return 1;
    if(require(s.active_probes==0,"active probes drained")) return 1;
    std::cout << "phase2 crawler tests passed\n";
}
