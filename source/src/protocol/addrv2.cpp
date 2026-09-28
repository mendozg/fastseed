#include "fastseed/protocol/addrv2.hpp"
#include <stdexcept>

namespace fastseed::protocol {

bool is_known_addrv2_network(std::uint8_t network_id) noexcept {
    switch (network_id) {
    case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
        return true;
    default:
        return false;
    }
}

bool is_supported_addrv2_network(std::uint8_t network_id) noexcept {
    // FastSeed v1.0.0 supports IPv4, IPv6, Tor v3 and I2P. Tor v2 is
    // intentionally rejected because it is obsolete.
    switch (network_id) {
    case 0x01: case 0x02: case 0x04: case 0x05:
        return true;
    default:
        return false;
    }
}

std::string_view addrv2_network_name(std::uint8_t network_id) noexcept {
    switch (network_id) {
    case 0x01: return "ipv4";
    case 0x02: return "ipv6";
    case 0x03: return "torv2";
    case 0x04: return "torv3";
    case 0x05: return "i2p";
    default: return "unknown";
    }
}

void validate_addrv2_address(std::uint8_t network_id,
                             std::span<const std::uint8_t> address) {
    if (address.size() > ADDRV2_MAX_ADDRESS_SIZE)
        throw std::length_error("addrv2 address exceeds limit");

    switch (network_id) {
    case 0x01:
        if (address.size() != 4) throw std::invalid_argument("IPv4 ADDRv2 requires 4 bytes");
        break;
    case 0x02:
        if (address.size() != 16) throw std::invalid_argument("IPv6 ADDRv2 requires 16 bytes");
        break;
    case 0x03:
        throw std::invalid_argument("Tor v2 ADDRv2 is unsupported");
    case 0x04:
        if (address.size() != ADDRV2_FIXED_TORV3_SIZE)
            throw std::invalid_argument("Tor v3 ADDRv2 requires 32 bytes");
        break;
    case 0x05:
        if (address.size() != ADDRV2_FIXED_I2P_SIZE)
            throw std::invalid_argument("I2P ADDRv2 requires 32 bytes");
        break;
    default:
        // Future network IDs remain forward-compatible, subject to 512 bytes.
        break;
    }
}

} // namespace fastseed::protocol
