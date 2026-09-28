#include "fastseed/protocol/conformance.hpp"
#include "fastseed/protocol/wire_messages.hpp"
#include <cassert>
#include <iostream>

using namespace fastseed::protocol;

int main() {
    assert(message_type("sendaddrv2") == MessageType::SendAddrV2);
    assert(canonical_command(MessageType::SendAddrV2) == "sendaddrv2");

    const auto payload = encode_sendaddrv2();
    assert(payload.empty());

    const auto wire = encode_message("sendaddrv2", payload);
    const std::vector<std::uint8_t> expected = {
        0xa2,0xb2,0xe2,0xf2,
        's','e','n','d','a','d','d','r','v','2',0,0,
        0,0,0,0,
        0x5d,0xf6,0xe0,0xe2
    };
    assert(wire == expected);

    const auto decoded = decode_message(wire);
    assert(command_string(decoded.header) == "sendaddrv2");
    assert(decoded.payload.empty());

    std::cout << "phase10.1 protocol tests: PASS\n";
}
