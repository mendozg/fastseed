#include "fastseed/protocol/probe.hpp"
#include "fastseed/protocol/wire_messages.hpp"
#include "fastseed/protocol/version_message.hpp"
#include "fastseed/protocol/message.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/crawler/crawler.hpp"
#include <boost/asio.hpp>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <stdexcept>

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

using namespace fastseed;

static std::vector<std::uint8_t> frame(std::string_view command, std::span<const std::uint8_t> payload = {}) {
    protocol::MessageHeader h;
    h.magic = bsty::MAINNET_MAGIC;
    std::copy(command.begin(), command.end(), h.command.begin());
    h.payload_size = static_cast<std::uint32_t>(payload.size());
    h.checksum = protocol::checksum(payload);
    auto out = protocol::encode_header(h);
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

static protocol::Message read_frame(boost::asio::ip::tcp::socket& sock) {
    std::array<std::uint8_t,24> hb{};
    boost::asio::read(sock, boost::asio::buffer(hb));
    auto h = protocol::decode_header(hb);
    std::vector<std::uint8_t> p(h.payload_size);
    if (!p.empty()) boost::asio::read(sock, boost::asio::buffer(p));
    return {h, std::move(p)};
}

int main() {
    boost::asio::io_context server_io;
    boost::asio::ip::tcp::acceptor acceptor(server_io, {boost::asio::ip::address_v4::loopback(), 0});
    const auto port = acceptor.local_endpoint().port();

    std::thread server([&] {
        boost::asio::ip::tcp::socket sock(server_io);
        acceptor.accept(sock);

        auto v = read_frame(sock);
        require(protocol::command_string(v.header) == "version", "expected version");
        auto recv_version = protocol::decode_version(v.payload);

        boost::asio::ip::address loop = boost::asio::ip::address_v4::loopback();
        protocol::NetworkAddress recv{0, loop, port};
        protocol::NetworkAddress from{0, loop, port};
        auto payload = protocol::encode_version(protocol::make_version(recv, from));
        auto vf = frame("version", payload);
        boost::asio::write(sock, boost::asio::buffer(vf));

        auto sendaddr = read_frame(sock);
        require(protocol::command_string(sendaddr.header) == "sendaddrv2", "expected sendaddrv2");
        require(sendaddr.payload.empty(), "sendaddrv2 payload must be empty");

        auto verack = read_frame(sock);
        require(protocol::command_string(verack.header) == "verack", "expected verack");
        auto vr = frame("verack");
        boost::asio::write(sock, boost::asio::buffer(vr));

        auto getaddr = read_frame(sock);
        require(protocol::command_string(getaddr.header) == "getaddr", "expected getaddr");

        protocol::AddrV2Address tor{1700000000u, 0, 0x04, std::vector<std::uint8_t>(32, 0x11), 8226};
        protocol::AddrV2Address i2p{1700000001u, 0, 0x05, std::vector<std::uint8_t>(32, 0x22), 8226};
        protocol::AddrV2Address ipv4{1700000002u, 0, 0x01, {203,0,113,9}, 8226};
        auto ap = protocol::encode_addrv2({tor, i2p, ipv4});
        auto af = frame("addrv2", ap);
        boost::asio::write(sock, boost::asio::buffer(af));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });

    boost::asio::io_context client_io;
    protocol::PeerProbe probe(client_io);
    bool completed = false;
    protocol::ProbeResult result;
    probe.run("127.0.0.1", port, std::chrono::milliseconds(1000), std::chrono::milliseconds(1000),
        [&](protocol::ProbeResult r) { result = std::move(r); completed = true; });
    client_io.run();
    server.join();

    require(completed, "probe did not complete");
    require(result.status == protocol::ProbeStatus::Success, "probe failed");
    require(result.discovered_addrv2.size() == 3, "expected three ADDRv2 records");
    require(result.discovered_addrv2[0].network_id == 0x04, "expected Tor v3");
    require(result.discovered_addrv2[1].network_id == 0x05, "expected I2P");
    require(result.discovered_addrv2[2].network_id == 0x01, "expected IPv4");

    // Verify the crawler consumes discovered ADDRv2 records and keeps
    // non-IP transports in AwaitingTransport instead of attempting raw TCP.
    const auto discovered = result.discovered_addrv2;
    struct FakeRunner final : crawler::ProbeRunner {
        std::vector<protocol::AddrV2Address> discovered;
        bool first = true;
        explicit FakeRunner(std::vector<protocol::AddrV2Address> d) : discovered(std::move(d)) {}
        void run(const std::string&, std::uint16_t,
                 std::chrono::milliseconds,
                 std::chrono::milliseconds,
                 std::function<void(protocol::ProbeResult)> done) override {
            protocol::ProbeResult r;
            r.status = protocol::ProbeStatus::Success;
            if (first) {
                r.discovered_addrv2 = discovered;
                first = false;
            }
            done(std::move(r));
        }
    };

    boost::asio::io_context crawler_io;
    crawler::PeerCrawler crawler(crawler_io, {}, std::make_unique<FakeRunner>(discovered));
    require(crawler.add_candidate("127.0.0.1", 8226, crawler::CandidateSource::Manual), "failed to add initial candidate");
    crawler.start();
    crawler_io.run();
    const auto snapshot = crawler.snapshot();
    bool saw_tor = false, saw_i2p = false, saw_ipv4 = false;
    for (const auto& c : snapshot) {
        if (c.network == crawler::CandidateNetwork::TorV3) {
            saw_tor = true;
            require(c.state == crawler::CandidateState::AwaitingTransport, "non-IP candidate must await transport");
        }
        if (c.network == crawler::CandidateNetwork::I2P) {
            saw_i2p = true;
            require(c.state == crawler::CandidateState::AwaitingTransport, "non-IP candidate must await transport");
        }
        if (c.network == crawler::CandidateNetwork::IPv4 && c.host == "203.0.113.9") {
            saw_ipv4 = true;
            require(c.state == crawler::CandidateState::Queued || c.state == crawler::CandidateState::Validated, "IPv4 candidate should enter crawler pipeline");
        }
    }
    require(saw_tor && saw_i2p && saw_ipv4, "crawler did not ingest all ADDRv2 networks");
    require(crawler.stats().addrv2_messages == 1, "expected one ADDRv2 message");
    require(crawler.stats().addrv2_addresses == 3, "expected three ADDRv2 addresses");

    std::cout << "phase10.4 ADDRv2 peer exchange tests: PASS\n";
}
