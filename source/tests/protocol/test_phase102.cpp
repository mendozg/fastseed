#include "fastseed/protocol/addrv2.hpp"
#include "fastseed/protocol/wire_messages.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace fastseed::protocol;

static std::vector<std::uint8_t> seq(std::uint8_t base) {
    std::vector<std::uint8_t> v(32);
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<std::uint8_t>(base + i);
    return v;
}

int main() {
    assert(is_supported_addrv2_network(0x01));
    assert(is_supported_addrv2_network(0x02));
    assert(is_supported_addrv2_network(0x04));
    assert(is_supported_addrv2_network(0x05));
    assert(!is_supported_addrv2_network(0x03));
    assert(addrv2_network_name(0x04) == "torv3");
    assert(addrv2_network_name(0x05) == "i2p");

    const auto tor = seq(0x10);
    const auto i2p = seq(0x80);
    std::vector<AddrV2Address> entries = {
        {1700000000U, 1, 0x01, {127,0,0,1}, 8226},
        {1700000001U, 1, 0x02, std::vector<std::uint8_t>(16, 0), 8226},
        {1700000002U, 1, 0x04, tor, 8226},
        {1700000003U, 1, 0x05, i2p, 8226},
    };

    const auto wire = encode_addrv2(entries);
    const auto decoded = decode_addrv2(wire);
    assert(decoded.size() == entries.size());
    for (std::size_t i = 0; i < entries.size(); ++i) {
        assert(decoded[i].timestamp == entries[i].timestamp);
        assert(decoded[i].services == entries[i].services);
        assert(decoded[i].network_id == entries[i].network_id);
        assert(decoded[i].address == entries[i].address);
        assert(decoded[i].port == entries[i].port);
    }

    bool threw = false;
    try { validate_addrv2_address(0x04, std::vector<std::uint8_t>(31)); }
    catch (const std::exception&) { threw = true; }
    assert(threw);

    threw = false;
    try { validate_addrv2_address(0x05, std::vector<std::uint8_t>(33)); }
    catch (const std::exception&) { threw = true; }
    assert(threw);

    threw = false;
    try { validate_addrv2_address(0x03, std::vector<std::uint8_t>(32)); }
    catch (const std::exception&) { threw = true; }
    assert(threw);

    // Unknown network IDs remain forward-compatible within the 512-byte bound.
    std::vector<AddrV2Address> future = {{1700000004U, 0, 0x42, {1,2,3}, 8226}};
    assert(decode_addrv2(encode_addrv2(future))[0].network_id == 0x42);

    // BIP155 CompactSize must be minimally encoded.
    const std::vector<std::uint8_t> noncanonical = {0xfd, 0x01, 0x00};
    threw = false;
    try { (void)decode_addrv2(noncanonical); }
    catch (const std::exception&) { threw = true; }
    assert(threw);

    std::cout << "phase10.2 addrv2 tests: PASS\n";
}
