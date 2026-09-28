#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace fastseed::protocol {

enum class AddrV2Network : std::uint8_t {
    IPv4 = 0x01,
    IPv6 = 0x02,
    TorV2 = 0x03,
    TorV3 = 0x04,
    I2P = 0x05,
};

constexpr std::size_t ADDRV2_MAX_ADDRESS_SIZE = 512;
constexpr std::size_t ADDRV2_MAX_ENTRIES = 1000;
constexpr std::size_t ADDRV2_FIXED_TORV3_SIZE = 32;
constexpr std::size_t ADDRV2_FIXED_I2P_SIZE = 32;

bool is_known_addrv2_network(std::uint8_t network_id) noexcept;
bool is_supported_addrv2_network(std::uint8_t network_id) noexcept;
std::string_view addrv2_network_name(std::uint8_t network_id) noexcept;

// Validates the address length rules for the known BIP155 network IDs.
// Unknown network IDs are accepted up to the protocol maximum so they can
// be preserved without making FastSeed reject future BIP155 extensions.
void validate_addrv2_address(std::uint8_t network_id,
                              std::span<const std::uint8_t> address);

} // namespace fastseed::protocol
