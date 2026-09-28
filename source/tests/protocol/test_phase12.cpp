#include "fastseed/protocol/wire_messages.hpp"
#include "fastseed/protocol/message.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/network_address.hpp"
#include <boost/asio/ip/address.hpp>
#include <cassert>
#include <iostream>

using namespace fastseed::protocol;

static NetworkAddress v4(const char* ip, std::uint16_t port) {
    return NetworkAddress{1, boost::asio::ip::make_address(ip), port};
}

static NetworkAddress v6(const char* ip, std::uint16_t port) {
    return NetworkAddress{1, boost::asio::ip::make_address(ip), port};
}

int main() {
    assert(message_type("version") == MessageType::Version);
    assert(message_type("verack") == MessageType::Verack);
    assert(message_type("sendaddrv2") == MessageType::SendAddrV2);
    assert(message_type("addr") == MessageType::Addr);
    assert(message_type("addrv2") == MessageType::AddrV2);
    assert(message_type("getaddr") == MessageType::GetAddr);
    assert(message_type("ping") == MessageType::Ping);
    assert(message_type("pong") == MessageType::Pong);
    assert(message_type("bogus") == MessageType::Unknown);

    auto a = v4("203.0.113.10", 8226);
    std::vector<PeerAddress> peers{{1700000000u, a}};
    auto addr = encode_addr(peers);
    auto decoded = decode_addr(addr);
    assert(decoded.size() == 1);
    assert(decoded[0].timestamp == 1700000000u);
    assert(decoded[0].address.port == 8226);
    assert(decoded[0].address.address.to_string() == "203.0.113.10");
    assert(encode_addr(decoded) == addr);

    AddrV2Address av4{1700000001u, 0, 1, {203,0,113,11}, 8226};
    AddrV2Address av6{1700000002u, 1, 2, {0x20,0x01,0x0d,0xb8,0,0,0,0,0,0,0,0,0,0,0,1}, 8226};
    auto av = encode_addrv2({av4,av6});
    auto avd = decode_addrv2(av);
    assert(avd.size() == 2);
    assert(avd[0].address.size() == 4 && avd[0].port == 8226);
    assert(avd[1].address.size() == 16 && avd[1].services == 1);
    assert(encode_addrv2(avd) == av);

    assert(encode_verack().empty());
    assert(encode_getaddr().empty());
    constexpr std::uint64_t nonce = 0x1122334455667788ULL;
    assert(decode_ping_nonce(encode_ping(nonce)) == nonce);
    assert(decode_pong_nonce(encode_pong(nonce)) == nonce);

    std::cout << "phase12 protocol tests: PASS\n";
    return 0;
}
