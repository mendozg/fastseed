#include "fastseed/config/runtime_config.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
int main() {
    const std::string good="phase9-good.toml";
    {
        std::ofstream f(good);
        f << "[dns]\nlisten_port = 53\nmax_response_size = 1232\n"
          << "[crawler]\nmax_concurrent_probes = 256\nmax_open_connections = 2048\n"
          << "[cache]\nhot_ipv4 = 1000\n[health]\nfreshness_seconds = 1800\n"
          << "[database]\npath = \"fastseed.sqlite3\"\n";
    }
    auto a=fastseed::config::validate_file(good);
    assert(a.ok); std::remove(good.c_str());
    const std::string bad="phase9-bad.toml";
    { std::ofstream f(bad); f << "[dns]\nlisten_port = 0\nmax_response_size = 0\n"; }
    auto b=fastseed::config::validate_file(bad);
    assert(!b.ok);
    bool port_error=false, size_error=false;
    for(const auto& error:b.errors) {
        port_error |= error.find("dns.listen_port")!=std::string::npos;
        size_error |= error.find("dns.max_response_size")!=std::string::npos;
    }
    assert(port_error && size_error); std::remove(bad.c_str());
    assert(fastseed::config::version_string()=="BSTY-FastSeed 1.0.0 (Production)");
    std::cout << "phase9_tests PASS\n";
}
