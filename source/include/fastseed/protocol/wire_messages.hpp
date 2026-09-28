#pragma once

#include "network_address.hpp"
#include "addrv2.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace fastseed::protocol {

enum class MessageType {
    Version,
    Verack,
    SendAddrV2,
    Addr,
    AddrV2,
    GetAddr,
    Ping,
    Pong,
    Unknown
};

struct PeerAddress {
    std::uint32_t timestamp{};
    NetworkAddress address;
};

struct AddrV2Address {
    std::uint32_t timestamp{};
    std::uint64_t services{};
    std::uint8_t network_id{};
    std::vector<std::uint8_t> address;
    std::uint16_t port{};
};

MessageType message_type(std::string_view command) noexcept;

std::vector<std::uint8_t> encode_verack();
std::vector<std::uint8_t> encode_sendaddrv2();
std::vector<PeerAddress> decode_addr(std::span<const std::uint8_t> payload);
std::vector<std::uint8_t> encode_addr(const std::vector<PeerAddress>& entries);
std::vector<AddrV2Address> decode_addrv2(std::span<const std::uint8_t> payload);
std::vector<std::uint8_t> encode_addrv2(const std::vector<AddrV2Address>& entries);
std::vector<std::uint8_t> encode_getaddr();
std::uint64_t decode_ping_nonce(std::span<const std::uint8_t> payload);
std::vector<std::uint8_t> encode_ping(std::uint64_t nonce);
std::uint64_t decode_pong_nonce(std::span<const std::uint8_t> payload);
std::vector<std::uint8_t> encode_pong(std::uint64_t nonce);

} // namespace fastseed::protocol
