#include "fastseed/protocol/wire_messages.hpp"
#include "fastseed/protocol/protocol_limits.hpp"
#include "fastseed/protocol/addrv2.hpp"
#include "fastseed/protocol/serializer.hpp"
#include "fastseed/protocol/protocol_error.hpp"
#include <boost/asio/ip/address.hpp>
#include <stdexcept>
#include <string_view>

namespace fastseed::protocol {
MessageType message_type(std::string_view command) noexcept {
    if (command == "version") return MessageType::Version;
    if (command == "verack") return MessageType::Verack;
    if (command == "sendaddrv2") return MessageType::SendAddrV2;
    if (command == "wtxidrelay") return MessageType::WtxidRelay;
    if (command == "addr") return MessageType::Addr;
    if (command == "addrv2") return MessageType::AddrV2;
    if (command == "getaddr") return MessageType::GetAddr;
    if (command == "ping") return MessageType::Ping;
    if (command == "pong") return MessageType::Pong;
    return MessageType::Unknown;
}

std::vector<std::uint8_t> encode_verack() { return {}; }
std::vector<std::uint8_t> encode_sendaddrv2() { return {}; }
std::vector<std::uint8_t> encode_wtxidrelay() { return {}; }
std::vector<std::uint8_t> encode_getaddr() { return {}; }

std::vector<PeerAddress> decode_addr(std::span<const std::uint8_t> payload) {
    Reader r(payload);
    const auto count = r.compact_size(ADDRV2_MAX_ENTRIES);
    std::vector<PeerAddress> out;
    out.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        PeerAddress e;
        e.timestamp = r.u32();
        e.address = read_network_address(r);
        out.push_back(std::move(e));
    }
    r.require_end();
    return out;
}

std::vector<std::uint8_t> encode_addr(const std::vector<PeerAddress>& entries) {
    if (entries.size() > ADDRV2_MAX_ENTRIES) throw std::length_error("too many addr entries");
    Writer w; w.compact_size(entries.size());
    for (const auto& e : entries) { w.u32(e.timestamp); write_network_address(w, e.address); }
    return w.data();
}

std::vector<AddrV2Address> decode_addrv2(std::span<const std::uint8_t> payload) {
    Reader r(payload);
    const auto count = r.compact_size(ADDRV2_MAX_ENTRIES);
    std::vector<AddrV2Address> out;
    out.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        AddrV2Address e;
        e.timestamp = r.u32();
        e.services = r.compact_size();
        e.network_id = r.u8();
        const auto len = r.compact_size(ADDRV2_MAX_ADDRESS_SIZE);
        e.address = r.bytes(static_cast<std::size_t>(len));
        e.port = r.be16();
        validate_addrv2_address(e.network_id, e.address);
        out.push_back(std::move(e));
    }
    r.require_end();
    return out;
}

std::vector<std::uint8_t> encode_addrv2(const std::vector<AddrV2Address>& entries) {
    if (entries.size() > ADDRV2_MAX_ENTRIES) throw std::length_error("too many addrv2 entries");
    Writer w; w.compact_size(entries.size());
    for (const auto& e : entries) {
        validate_addrv2_address(e.network_id, e.address);
        w.u32(e.timestamp); w.compact_size(e.services); w.u8(e.network_id); w.compact_size(e.address.size()); w.bytes(e.address); w.be16(e.port);
    }
    return w.data();
}

std::uint64_t decode_ping_nonce(std::span<const std::uint8_t> payload) {
    if (payload.size() != 8) throw ProtocolError(ProtocolErrorCode::InvalidPayload,"invalid ping payload size");
    Reader r(payload); return r.u64();
}
std::vector<std::uint8_t> encode_ping(std::uint64_t nonce) { Writer w; w.u64(nonce); return w.data(); }
std::uint64_t decode_pong_nonce(std::span<const std::uint8_t> payload) {
    if (payload.size() != 8) throw ProtocolError(ProtocolErrorCode::InvalidPayload,"invalid pong payload size");
    Reader r(payload); return r.u64();
}
std::vector<std::uint8_t> encode_pong(std::uint64_t nonce) { Writer w; w.u64(nonce); return w.data(); }
}
