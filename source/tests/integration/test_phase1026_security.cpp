#include "fastseed/protocol/parser.hpp"
#include "fastseed/protocol/addrv2.hpp"
#include "fastseed/protocol/wire_messages.hpp"
#include "fastseed/protocol/serializer.hpp"
#include "fastseed/crawler/crawler.hpp"
#include "fastseed/dns/rate_limiter.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace fastseed;
using namespace std::chrono_literals;

static std::vector<std::uint8_t> packet(std::uint32_t payload_size) {
    protocol::Writer w;
    w.u8(0xA2); w.u8(0xB2); w.u8(0xE2); w.u8(0xF2);
    const char cmd[12] = {'p','i','n','g',0,0,0,0,0,0,0,0};
    w.bytes(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(cmd),12));
    w.u32(payload_size);
    w.u32(0);
    return w.data();
}

int main() {
    // Parser must reject oversized messages before attempting allocation.
    try { (void)protocol::Parser{}.parse(packet(5 * 1024 * 1024)); assert(false); }
    catch (const protocol::ProtocolError&) {}

    // Command padding must be canonical: non-zero bytes after NUL are rejected.
    auto bad = packet(0);
    bad[16] = 0x7f;
    try { (void)protocol::Parser{}.parse(bad); assert(false); }
    catch (const protocol::ProtocolError&) {}

    // ADDRv2 remains strictly bounded and rejects obsolete Tor v2.
    protocol::AddrV2Address tor2{};
    tor2.network_id = 0x03;
    tor2.address.assign(32, 1);
    try { protocol::validate_addrv2_address(tor2.network_id, tor2.address); assert(false); }
    catch (const std::exception&) {}
    protocol::AddrV2Address huge{};
    huge.network_id = 0x99;
    huge.address.assign(513, 0);
    try { protocol::validate_addrv2_address(huge.network_id, huge.address); assert(false); }
    catch (const std::exception&) {}

    // Candidate admission rejects pathological host strings and remains bounded.
    boost::asio::io_context io;
    crawler::CrawlerConfig cc;
    cc.max_candidates = 4;
    crawler::PeerCrawler crawler(io, cc);
    assert(!crawler.add_candidate(std::string(256, 'x'), 8226, crawler::CandidateSource::Manual));
    for (int i = 0; i < 4; ++i)
        assert(crawler.add_candidate("192.0.2." + std::to_string(i + 1), 8226, crawler::CandidateSource::Manual));
    assert(!crawler.add_candidate("192.0.2.99", 8226, crawler::CandidateSource::Manual));
    assert(crawler.stats().candidates_total == 4);

    // DNS rate limiting: an over-quota source must not consume global tokens.
    dns::RateLimiter::Config rc;
    rc.per_source_per_second = 1;
    rc.global_per_second = 2;
    rc.max_sources = 8;
    dns::RateLimiter limiter(rc);
    const auto t = std::chrono::steady_clock::now();
    assert(limiter.allow("attacker", t));
    assert(!limiter.allow("attacker", t + 1ms));
    assert(limiter.allow("legitimate", t + 2ms));
    assert(!limiter.allow("other", t + 3ms));
    assert(!limiter.allow(std::string(256, 'x'), t + 4ms));

    std::cout << "phase1026 security hardening/adversarial tests: PASS\n";
    return 0;
}
