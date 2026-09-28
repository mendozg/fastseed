#include "fastseed/protocol/conformance.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/protocol_error.hpp"
#include <cassert>
#include <iostream>
#include <string_view>

using namespace fastseed::protocol;

int main() {
    assert(canonical_command(MessageType::Version) == "version");
    assert(canonical_command(MessageType::Verack) == "verack");
    assert(canonical_command(MessageType::SendAddrV2) == "sendaddrv2");
    assert(canonical_command(MessageType::AddrV2) == "addrv2");
    assert(canonical_command(MessageType::Unknown).empty());

    const auto verack = encode_message("verack", {});
    const std::vector<std::uint8_t> expected = {
        0xa2,0xb2,0xe2,0xf2,
        'v','e','r','a','c','k',0,0,0,0,0,0,
        0,0,0,0,
        0x5d,0xf6,0xe0,0xe2
    };
    assert(verack == expected);
    const auto decoded = decode_message(verack);
    assert(command_string(decoded.header) == "verack");
    assert(decoded.payload.empty());

    const std::vector<std::uint8_t> payload = {1,2,3,4};
    const auto wire = encode_message("ping", payload);
    const auto msg = decode_message(wire);
    assert(command_string(msg.header) == "ping");
    assert(msg.payload == payload);

    HandshakeStateMachine sm;
    assert(sm.state() == HandshakeState::Disconnected);
    assert(sm.transition(HandshakeEvent::BeginConnect));
    assert(sm.transition(HandshakeEvent::VersionSent));
    assert(sm.transition(HandshakeEvent::VersionReceived));
    assert(sm.transition(HandshakeEvent::VerackSent));
    assert(sm.transition(HandshakeEvent::VerackReceived));
    assert(sm.state() == HandshakeState::Established);
    assert(!sm.transition(HandshakeEvent::VersionSent));

    constexpr std::uint64_t nonce = 0x1122334455667788ULL;
    require_pong_nonce(nonce, encode_pong(nonce));

    bool mismatch = false;
    try { require_pong_nonce(nonce + 1, encode_pong(nonce)); }
    catch (const ProtocolError&) { mismatch = true; }
    assert(mismatch);

    bool bad_command = false;
    try { (void)encode_message(std::string_view("bad\0cmd", 7), {}); }
    catch (const ProtocolError&) { bad_command = true; }
    assert(bad_command);

    std::cout << "phase13 protocol tests: PASS\n";
}
