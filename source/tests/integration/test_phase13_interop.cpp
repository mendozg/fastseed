#include "fastseed/protocol/conformance.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/version_message.hpp"
#include <boost/asio.hpp>
#include <cassert>
#include <iostream>
#include <thread>

using namespace fastseed::protocol;
using namespace fastseed::bsty;
using boost::asio::ip::tcp;

int main() {
    boost::asio::io_context server_io;
    tcp::acceptor acceptor(server_io, tcp::endpoint(tcp::v4(), 0));
    const auto port = acceptor.local_endpoint().port();
    std::exception_ptr server_error;

    std::thread server([&] {
        try {
            tcp::socket sock(server_io);
            acceptor.accept(sock);

            std::array<std::uint8_t, 24> hbuf{};
            boost::asio::read(sock, boost::asio::buffer(hbuf));
            auto h = decode_header(hbuf);
            assert(command_string(h) == "version");
            std::vector<std::uint8_t> payload(h.payload_size);
            boost::asio::read(sock, boost::asio::buffer(payload));
            assert(checksum(payload) == h.checksum);
            auto v = decode_version(payload);
            assert(v.version == PROTOCOL_VERSION);
            assert(v.user_agent == "/BSTYFastSeed:0.1/");

            const auto remote_version = encode_version(make_version(
                v.addr_recv, v.addr_from));
            const auto version_wire = encode_message("version", remote_version);
            boost::asio::write(sock, boost::asio::buffer(version_wire));

            boost::asio::read(sock, boost::asio::buffer(hbuf));
            h = decode_header(hbuf);
            assert(command_string(h) == "verack");
            assert(h.payload_size == 0);
            assert(h.checksum == checksum({}));
            assert(decode_message(std::vector<std::uint8_t>(hbuf.begin(), hbuf.end())).payload.empty());

            const auto verack_wire = encode_message("verack", {});
            boost::asio::write(sock, boost::asio::buffer(verack_wire));
        } catch (...) {
            server_error = std::current_exception();
        }
    });

    // Verify the exact network interaction independently of PeerProbe so this
    // test remains deterministic and does not require a public BSTY node.
    tcp::socket client(server_io);
    client.connect(tcp::endpoint(boost::asio::ip::address_v4::loopback(), port));

    auto version_payload = encode_version(make_version(
        NetworkAddress{0, boost::asio::ip::address_v4::loopback(), port},
        NetworkAddress{0, boost::asio::ip::address_v4::loopback(), port}));
    const auto version_wire = encode_message("version", version_payload);
    boost::asio::write(client, boost::asio::buffer(version_wire));

    std::array<std::uint8_t, 24> hbuf{};
    boost::asio::read(client, boost::asio::buffer(hbuf));
    auto h = decode_header(hbuf);
    std::vector<std::uint8_t> payload(h.payload_size);
    boost::asio::read(client, boost::asio::buffer(payload));
    auto peer_version = decode_version(payload);
    assert(peer_version.version == PROTOCOL_VERSION);

    boost::asio::write(client, boost::asio::buffer(encode_message("verack", {})));
    boost::asio::read(client, boost::asio::buffer(hbuf));
    h = decode_header(hbuf);
    assert(command_string(h) == "verack");
    assert(h.payload_size == 0);

    boost::system::error_code ec;
    client.close(ec);
    acceptor.close(ec);
    server.join();
    if (server_error) std::rethrow_exception(server_error);

    std::cout << "phase13 interoperability: PASS\n";
}
