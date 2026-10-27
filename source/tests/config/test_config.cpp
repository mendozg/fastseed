#include "fastseed/config/runtime_config.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    std::vector<std::string> errors;
    auto cfg = fastseed::config::load_file("config/fastseed.production.toml", errors);
    assert(cfg.has_value());
    assert(errors.empty());
    assert(cfg->network.p2p_port == 8226);
    assert(cfg->network.protocol_version == 70016);
    assert(cfg->crawler.max_concurrent_probes == 256);
    assert(cfg->crawler.scheduler.direct_tcp.max_inflight == 192);
    assert(cfg->crawler.scheduler.tor_socks5.max_inflight == 32);
    assert(cfg->crawler.scheduler.i2p_sam.max_inflight == 32);
    assert(cfg->crawler.feedback.global_max_inflight == 256);
    assert(cfg->crawler.feedback.direct_tcp.target == 192);
    assert(cfg->crawler.feedback.tor_socks5.target == 32);
    assert(cfg->crawler.tor_transport.proxy_port == 9050);
    assert(cfg->crawler.i2p_transport.sam_port == 7656);
    assert(cfg->dns.server.port == 53);
    assert(cfg->dns.engine.zone == "seed.globalboost.info");
    assert(cfg->dns.engine.rate_limit.global_per_second == 5000);
    assert(cfg->database.max_peers == 250000);
    assert(cfg->persistence.max_items == 10000);
    assert(cfg->persistence.batch_size == 128);
    assert(cfg->persistence.flush_interval.count() == 500);
    assert(cfg->intelligence.minimum_observations == 3);

    const std::string good = "config-test-good.toml";
    {
        std::ofstream f(good);
        f << "[network]\nnetwork = \"mainnet\"\np2p_port = 8226\n"
          << "[dns]\nlisten_address = \"127.0.0.1\"\nlisten_port = 5353\n"
          << "max_answers = 8\nmax_response_size = 1232\n"
          << "[crawler]\nmax_concurrent_probes = 4\nmax_open_connections = 8\nmax_candidates = 100\n"
          << "[cache]\nhot_ipv4 = 4\n[health]\nfreshness_seconds = 1800\n"
          << "[database]\npath = \"test.db\"\n";
    }
    errors.clear();
    cfg = fastseed::config::load_file(good, errors);
    assert(cfg.has_value());
    assert(cfg->dns.server.bind_address.to_string() == "127.0.0.1");
    assert(cfg->dns.server.port == 5353);
    std::remove(good.c_str());

    const std::string bad = "config-test-bad.toml";
    {
        std::ofstream f(bad);
        f << "[dns]\nlisten_port = 0\nunknown = 1\n"
          << "[crawler]\nmax_concurrent_probes = 0\n"
          << "[database]\npath = \"\"\n";
    }
    errors.clear();
    cfg = fastseed::config::load_file(bad, errors);
    assert(!cfg.has_value());
    assert(errors.size() >= 4);
    std::remove(bad.c_str());

    std::cout << "phase_config_tests: PASS\n";
}
