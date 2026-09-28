#include "fastseed/config/runtime_config.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
int main(){
 const std::string good="phase9-good.toml"; {std::ofstream f(good);f<<"[dns]\nlisten_port = 53\nmax_response_size = 1232\n[crawler]\nmax_concurrent_probes = 256\nmax_open_connections = 2048\n[cache]\nhot_ipv4 = 1000\n[health]\nfreshness_seconds = 1800\n[database]\npath = \\\"fastseed.sqlite3\\\"\n";} auto a=fastseed::config::validate_file(good); assert(a.ok); std::remove(good.c_str());
 const std::string bad="phase9-bad.toml"; {std::ofstream f(bad);f<<"[dns]\nlisten_port = 0\nmax_response_size = 0\n";} auto b=fastseed::config::validate_file(bad); assert(!b.ok && b.errors.size()>=6); std::remove(bad.c_str());
 assert(fastseed::config::version_string().find("0.9.0")!=std::string::npos); std::cout<<"phase9_tests PASS\n";
}
