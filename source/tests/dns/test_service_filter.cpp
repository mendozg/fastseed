#include "fastseed/dns/native_server.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace fastseed;
using Bytes = std::vector<std::uint8_t>;
namespace {
constexpr auto zone = "seed.globalboost.info";
int checks = 0;
void check(bool value, const char* label) {
    ++checks;
    if (!value) throw std::runtime_error(label);
}
unsigned read16(const Bytes& bytes, std::size_t pos) {
    return (unsigned(bytes.at(pos)) << 8) | bytes.at(pos + 1);
}
Bytes query(std::string name, unsigned type = dns::TYPE_A, unsigned udp_size = 1232) {
    Bytes out{0x12, 0x34, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
    for (std::size_t start = 0; start < name.size();) {
        auto end = name.find('.', start);
        if (end == std::string::npos) end = name.size();
        out.push_back(static_cast<std::uint8_t>(end - start));
        out.insert(out.end(), name.begin() + start, name.begin() + end);
        start = end + 1;
    }
    out.insert(out.end(), {0, 0, static_cast<std::uint8_t>(type), 0, 1});
    if (udp_size) {
        out[11] = 1;
        out.insert(out.end(), {0, 0, 41, static_cast<std::uint8_t>(udp_size >> 8),
            static_cast<std::uint8_t>(udp_size), 0, 0, 0, 0, 0, 0});
    }
    return out;
}
// Independent decoder: verify actual owners and RDATA, not just answer counts.
std::string name_at(const Bytes& bytes, std::size_t& pos, unsigned depth = 0) {
    if (depth > 20) throw std::runtime_error("compression loop in response");
    std::string name;
    while (const auto length = bytes.at(pos++)) {
        if (!name.empty()) name += '.';
        if ((length & 0xc0) == 0xc0) {
            std::size_t target = ((length & 63) << 8) | bytes.at(pos++);
            name += name_at(bytes, target, depth + 1);
            return name;
        }
        if (length > 63 || pos + length > bytes.size()) throw std::runtime_error("invalid label");
        name.append(bytes.begin() + pos, bytes.begin() + pos + length);
        pos += length;
    }
    return name;
}
struct Record { std::string owner; unsigned type; Bytes data; };
std::vector<Record> answers(const Bytes& bytes) {
    std::size_t pos = 12;
    name_at(bytes, pos); pos += 4;
    std::vector<Record> result;
    for (unsigned i = 0; i < read16(bytes, 6); ++i) {
        auto owner = name_at(bytes, pos);
        const auto type = read16(bytes, pos), length = read16(bytes, pos + 8);
        pos += 10;
        if (pos + length > bytes.size()) throw std::runtime_error("truncated answer");
        result.push_back({owner, type, Bytes(bytes.begin() + pos, bytes.begin() + pos + length)});
        pos += length;
    }
    return result;
}
std::vector<std::string> hosts(const Bytes& bytes) {
    std::vector<std::string> out;
    for (const auto& record : answers(bytes)) {
        if (record.type == dns::TYPE_A && record.data.size() == 4) {
            boost::asio::ip::address_v4::bytes_type address{};
            std::copy(record.data.begin(), record.data.end(), address.begin());
            out.push_back(boost::asio::ip::address_v4(address).to_string());
        } else if (record.type == dns::TYPE_AAAA && record.data.size() == 16) {
            boost::asio::ip::address_v6::bytes_type address{};
            std::copy(record.data.begin(), record.data.end(), address.begin());
            out.push_back(boost::asio::ip::address_v6(address).to_string());
        } else throw std::runtime_error("unexpected address RDATA");
    }
    std::sort(out.begin(), out.end());
    return out;
}
void add(health::PeerHealthCache& cache, const std::string& host, std::uint64_t services) {
    crawler::PeerCandidate candidate{host, 8226, crawler::CandidateSource::Manual};
    protocol::ProbeResult ok{};
    ok.status = protocol::ProbeStatus::Success;
    ok.services = services;
    ok.protocol_version = 70016;
    for (int i = 0; i < 3; ++i) cache.observe_success(candidate, ok);
}
void nodata(const Bytes& reply) {
    check((read16(reply, 2) & 0x40f) == 0x400 && read16(reply, 6) == 0 && read16(reply, 8) == 1,
          "known filter without records is authoritative NOERROR/SOA");
    std::size_t pos = 12; name_at(reply, pos); pos += 4;
    check(name_at(reply, pos) == zone && read16(reply, pos) == 6, "negative SOA owner is zone apex");
}
}

int main() {
    try {
        health::PeerHealthCache cache;
        add(cache, "192.0.2.1", 1);
        add(cache, "192.0.2.2", 8);
        add(cache, "192.0.2.3", 9);
        add(cache, "192.0.2.4", 1033); // 0x409: a superset must match x9.
        add(cache, "192.0.2.5", 0);
        add(cache, "2001:db8::9", 9);
        add(cache, "2001:db8::1", 1);
        dns::DnsResponseEngine engine;
        engine.publish_from_cache(cache, 42);
        const auto x9 = std::string("x9.") + zone;
        auto reply = engine.build_response(query(x9));
        check((read16(reply, 2) & 15) == 0, "wallet x9 query must not return NXDOMAIN");
        check(hosts(reply) == std::vector<std::string>{"192.0.2.3", "192.0.2.4"}, "x9 requires both service bits");
        check((read16(reply, 2) & 0x600) == 0x400, "filtered answer is authoritative and not truncated");
        for (const auto& record : answers(reply)) check(record.owner == x9, "answer owner is filtered name");
        check(hosts(engine.build_response(query(zone))).size() == 5, "apex remains unfiltered");
        check(hosts(engine.build_response(query(std::string("x0.") + zone))).size() == 5, "zero mask is unfiltered");
        check(hosts(engine.build_response(query(std::string("x1.") + zone))) ==
              std::vector<std::string>{"192.0.2.1", "192.0.2.3", "192.0.2.4"}, "single-bit filter");
        check(hosts(engine.build_response(query(std::string("x409.") + zone))) ==
              std::vector<std::string>{"192.0.2.4"}, "hexadecimal service mask");
        check(hosts(engine.build_response(query(x9, dns::TYPE_AAAA))) ==
              std::vector<std::string>{"2001:db8::9"}, "IPv6 peers filtered by services");
        for (const auto& name : {std::string("X0000000000000009.SeEd.GlObAlBoOsT.InFo"), x9 + "."}) {
            reply = engine.build_response(query(name));
            check(hosts(reply).size() == 2, "case, leading zeros and trailing dot");
        }
        reply = engine.build_response(query("X9.SeEd.GlObAlBoOsT.InFo"));
        check(answers(reply).at(0).owner == "X9.SeEd.GlObAlBoOsT.InFo", "response preserves original case");
        nodata(engine.build_response(query(std::string("x40.") + zone)));
        for (const auto type : {2u, 6u, 16u, 255u}) nodata(engine.build_response(query(x9, type)));
        for (const auto type : {2u, 6u}) {
            reply = engine.build_response(query(zone, type));
            check(read16(reply, 6) == 1 && read16(reply, 8) == 0, "apex SOA/NS unchanged");
        }
        for (const auto& label : {"child", "x", "xg", "x-1", "x+9", "x0x9", "x10000000000000000",
                                  "x00000000000000009", "x9.extra", "extra.x9"}) {
            reply = engine.build_response(query(std::string(label) + "." + zone));
            check((read16(reply, 2) & 0x40f) == 0x403 && read16(reply, 8) == 1,
                  "invalid or nested filter is NXDOMAIN/SOA");
        }
        for (const auto& name : {"x9.seed.globalboost.info.evil", "x9.notseed.globalboost.info", "x9.other.example"}) {
            reply = engine.build_response(query(name));
            check((read16(reply, 2) & 0x40f) == 5, "outside zone remains REFUSED");
        }
        health::PeerHealthCache wide;
        add(wide, "192.0.2.6", std::uint64_t{1} << 63);
        add(wide, "192.0.2.7", std::numeric_limits<std::uint64_t>::max());
        dns::DnsResponseEngine wide_engine; wide_engine.publish_from_cache(wide);
        check(hosts(wide_engine.build_response(query(std::string("x8000000000000000.") + zone))).size() == 2,
              "unsigned bit 63 accepted");
        check(hosts(wide_engine.build_response(query(std::string("xFFFFFFFFFFFFFFFF.") + zone))) ==
              std::vector<std::string>{"192.0.2.7"}, "all 64 bits parsed without overflow");
        dns::DnsResponseEngine empty; nodata(empty.build_response(query(x9)));
        // Matching peers must remain available after more than 4 * max_answers non-matches.
        health::PeerHealthCache crowded;
        add(crowded, "192.0.2.200", 9);
        for (int i = 1; i <= 20; ++i) add(crowded, "192.0.2." + std::to_string(i), 1);
        dns::DnsConfig small; small.max_answers = 1;
        dns::DnsResponseEngine small_engine(small); small_engine.publish_from_cache(crowded);
        check(hosts(small_engine.build_response(query(x9))) == std::vector<std::string>{"192.0.2.200"},
              "service selection occurs before any answer/snapshot cap");
        check(read16(small_engine.build_response(query(zone)), 6) == 1, "answer cap retained");
        // Large filtered RRset: UDP truncates the whole set; TCP returns every matching record.
        health::PeerHealthCache many;
        for (int i = 1; i <= 80; ++i) add(many, "198.51.100." + std::to_string(i), 9);
        dns::DnsConfig large; large.max_answers = 100;
        dns::DnsResponseEngine large_engine(large); large_engine.publish_from_cache(many);
        for (const auto size : {0u, 128u, 512u, 1232u, 4096u}) {
            const auto request = query(x9, dns::TYPE_A, size);
            reply = large_engine.build_response(request);
            const auto limit = size ? std::min(1232u, std::max(512u, size)) : 512u;
            check(reply.size() <= limit && (read16(reply, 2) & 0x200) && read16(reply, 6) == 0,
                  "filtered UDP RRset respects negotiation and truncates atomically");
            reply = large_engine.build_response(request, true);
            check(hosts(reply).size() == 80 && !(read16(reply, 2) & 0x200), "filtered TCP bypasses UDP size cap");
            check(read16(reply, 10) == unsigned(size != 0), "OPT only when requested");
        }
        // Real UDP and persistent TCP listeners; unit checks above independently establish semantics.
        boost::asio::io_context io;
        boost::asio::ip::tcp::acceptor probe(io, {boost::asio::ip::address_v4::loopback(), 0});
        const auto port = probe.local_endpoint().port(); probe.close();
        dns::NativeDnsServer::Config config;
        config.port = port; config.bind_address = boost::asio::ip::address_v4::loopback();
        dns::NativeDnsServer server(io, engine, config); server.start();
        std::thread worker([&] { io.run(); });
        try {
            boost::asio::io_context client;
            boost::asio::ip::udp::socket udp(client, boost::asio::ip::udp::v4());
            boost::asio::ip::tcp::socket tcp(client); tcp.connect({config.bind_address, port});
            for (const auto size : {0u, 512u, 1232u}) {
                for (const auto type : {dns::TYPE_A, dns::TYPE_AAAA}) {
                    for (const auto& name : {x9, std::string("x40.") + zone, std::string("xg.") + zone}) {
                        const auto request = query(name, type, size);
                        udp.send_to(boost::asio::buffer(request), {config.bind_address, port});
                        Bytes buffer(4096); boost::asio::ip::udp::endpoint remote;
                        const auto length = udp.receive_from(boost::asio::buffer(buffer), remote); buffer.resize(length);
                        check(buffer == engine.build_response(request), "real UDP filtered wire response");
                        Bytes frame{static_cast<std::uint8_t>(request.size() >> 8), static_cast<std::uint8_t>(request.size())};
                        frame.insert(frame.end(), request.begin(), request.end());
                        boost::asio::write(tcp, boost::asio::buffer(frame));
                        std::array<std::uint8_t, 2> prefix{}; boost::asio::read(tcp, boost::asio::buffer(prefix));
                        buffer.resize((unsigned(prefix[0]) << 8) | prefix[1]);
                        boost::asio::read(tcp, boost::asio::buffer(buffer));
                        check(buffer == engine.build_response(request, true), "real persistent TCP filtered wire response");
                    }
                }
            }
            tcp.close();
        } catch (...) { io.stop(); worker.join(); throw; }
        io.stop(); worker.join(); server.stop();
        std::cout << "DNS service filtering: " << checks << " checks PASS\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
