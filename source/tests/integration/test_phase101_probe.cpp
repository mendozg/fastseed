#include "fastseed/protocol/probe.hpp"
#include "fastseed/protocol/conformance.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/version_message.hpp"
#include <boost/asio.hpp>
#include <cassert>
#include <exception>
#include <iostream>
#include <thread>

using namespace fastseed::protocol;
using namespace fastseed::bsty;
using boost::asio::ip::tcp;

static Message read_message(tcp::socket& sock) {
    std::array<std::uint8_t, 24> hbuf{};
    boost::asio::read(sock, boost::asio::buffer(hbuf));
    auto h = decode_header(hbuf);
    std::vector<std::uint8_t> payload(h.payload_size);
    boost::asio::read(sock, boost::asio::buffer(payload));
    assert(checksum(payload) == h.checksum);
    std::vector<std::uint8_t> wire(hbuf.begin(), hbuf.end());
    wire.insert(wire.end(), payload.begin(), payload.end());
    return decode_message(wire);
}

int main() {
    boost::asio::io_context server_io;
    tcp::acceptor acceptor(server_io, tcp::endpoint(tcp::v4(), 0));
    const auto port = acceptor.local_endpoint().port();
    std::exception_ptr server_error;

    std::thread server([&] {
        try {
            tcp::socket sock(server_io);
            acceptor.accept(sock);

            const auto client_version = read_message(sock);
            assert(command_string(client_version.header) == "version");
            const auto v = decode_version(client_version.payload);
            assert(v.version == PROTOCOL_VERSION);

            const auto server_version = encode_message(
                "version", encode_version(make_version(v.addr_recv, v.addr_from)));
            boost::asio::write(sock, boost::asio::buffer(server_version));

            const auto capability = read_message(sock);
            assert(command_string(capability.header) == "sendaddrv2");
            assert(capability.payload.empty());

            const auto verack = read_message(sock);
            assert(command_string(verack.header) == "verack");
            assert(verack.payload.empty());

            const auto reply = encode_message("verack", {});
            boost::asio::write(sock, boost::asio::buffer(reply));
        } catch (...) {
            server_error = std::current_exception();
        }
    });

    boost::asio::io_context client_io;
    PeerProbe probe(client_io);
    ProbeResult result;
    bool completed = false;
    probe.run("127.0.0.1", port,
              std::chrono::milliseconds(1500),
              std::chrono::milliseconds(2500),
              [&](ProbeResult r) {
                  result = std::move(r);
                  completed = true;
              });
    client_io.run();

    server.join();
    if (server_error) std::rethrow_exception(server_error);

    assert(completed);
    assert(result.status == ProbeStatus::Success);
    assert(result.protocol_version == PROTOCOL_VERSION);

    std::cout << "phase10.1 probe interoperability: PASS\n";
}
