#include "fastseed/protocol/wire_messages.hpp"
#include "fastseed/protocol/protocol_limits.hpp"
#include "fastseed/protocol/addrv2.hpp"
#include "fastseed/protocol/serializer.hpp"
#include <boost/asio/ip/address.hpp>
#include <stdexcept>
#include <string_view>

namespace fastseed::protocol {
namespace {


void compact_size(Writer& w, std::uint64_t n) {
    if (n < 253) { w.u8(static_cast<std::uint8_t>(n)); return; }
    if (n <= 0xffff) { w.u8(253); w.u8(static_cast<std::uint8_t>(n)); w.u8(static_cast<std::uint8_t>(n >> 8)); return; }
    if (n <= 0xffffffffULL) { w.u8(254); w.u32(static_cast<std::uint32_t>(n)); return; }
    w.u8(255); w.u64(n);
}
std::uint64_t read_compact_size(Reader& r) {
    const auto marker = r.u8();
    if (marker < 253) return marker;
    if (marker == 253) {
        const auto v = static_cast<std::uint64_t>(r.u8()) |
                       (static_cast<std::uint64_t>(r.u8()) << 8);
        if (v < 253) throw std::runtime_error("non-canonical CompactSize");
        return v;
    }
    if (marker == 254) {
        const auto v = static_cast<std::uint64_t>(r.u32());
        if (v <= 0xffffULL) throw std::runtime_error("non-canonical CompactSize");
        return v;
    }
    const auto v = r.u64();
    if (v <= 0xffffffffULL) throw std::runtime_error("non-canonical CompactSize");
    return v;
}
void check_count(std::uint64_t n) {
    if (n > ADDRV2_MAX_ENTRIES) throw std::runtime_error("address count exceeds limit");
}
void write_be16(Writer& w, std::uint16_t p) { w.u8(static_cast<std::uint8_t>(p >> 8)); w.u8(static_cast<std::uint8_t>(p)); }
std::uint16_t read_be16(Reader& r) { return static_cast<std::uint16_t>(r.u8()) << 8 | r.u8(); }
}

MessageType message_type(std::string_view command) noexcept {
    if (command == "version") return MessageType::Version;
    if (command == "verack") return MessageType::Verack;
    if (command == "sendaddrv2") return MessageType::SendAddrV2;
    if (command == "addr") return MessageType::Addr;
    if (command == "addrv2") return MessageType::AddrV2;
    if (command == "getaddr") return MessageType::GetAddr;
    if (command == "ping") return MessageType::Ping;
    if (command == "pong") return MessageType::Pong;
    return MessageType::Unknown;
}

std::vector<std::uint8_t> encode_verack() { return {}; }
std::vector<std::uint8_t> encode_sendaddrv2() { return {}; }
std::vector<std::uint8_t> encode_getaddr() { return {}; }

std::vector<PeerAddress> decode_addr(std::span<const std::uint8_t> payload) {
    Reader r(payload);
    const auto count = read_compact_size(r);
    check_count(count);
    std::vector<PeerAddress> out;
    out.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        PeerAddress e;
        e.timestamp = r.u32();
        e.address = read_network_address(r);
        out.push_back(std::move(e));
    }
    if (r.remaining() != 0) throw std::runtime_error("trailing bytes in addr payload");
    return out;
}

std::vector<std::uint8_t> encode_addr(const std::vector<PeerAddress>& entries) {
    if (entries.size() > ADDRV2_MAX_ENTRIES) throw std::length_error("too many addr entries");
    Writer w; compact_size(w, entries.size());
    for (const auto& e : entries) { w.u32(e.timestamp); write_network_address(w, e.address); }
    return w.data();
}

std::vector<AddrV2Address> decode_addrv2(std::span<const std::uint8_t> payload) {
    Reader r(payload);
    const auto count = read_compact_size(r);
    check_count(count);
    std::vector<AddrV2Address> out;
    out.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        AddrV2Address e;
        e.timestamp = r.u32();
        e.services = read_compact_size(r);
        e.network_id = r.u8();
        const auto len = read_compact_size(r);
        if (len > ADDRV2_MAX_ADDRESS_SIZE) throw std::runtime_error("addrv2 address exceeds limit");
        e.address = r.bytes(static_cast<std::size_t>(len));
        e.port = read_be16(r);
        validate_addrv2_address(e.network_id, e.address);
        out.push_back(std::move(e));
    }
    if (r.remaining() != 0) throw std::runtime_error("trailing bytes in addrv2 payload");
    return out;
}

std::vector<std::uint8_t> encode_addrv2(const std::vector<AddrV2Address>& entries) {
    if (entries.size() > ADDRV2_MAX_ENTRIES) throw std::length_error("too many addrv2 entries");
    Writer w; compact_size(w, entries.size());
    for (const auto& e : entries) {
        validate_addrv2_address(e.network_id, e.address);
        w.u32(e.timestamp); compact_size(w, e.services); w.u8(e.network_id); compact_size(w, e.address.size()); w.bytes(e.address); write_be16(w, e.port);
    }
    return w.data();
}

std::uint64_t decode_ping_nonce(std::span<const std::uint8_t> payload) {
    if (payload.size() != 8) throw std::runtime_error("invalid ping payload size");
    Reader r(payload); return r.u64();
}
std::vector<std::uint8_t> encode_ping(std::uint64_t nonce) { Writer w; w.u64(nonce); return w.data(); }
std::uint64_t decode_pong_nonce(std::span<const std::uint8_t> payload) {
    if (payload.size() != 8) throw std::runtime_error("invalid pong payload size");
    Reader r(payload); return r.u64();
}
std::vector<std::uint8_t> encode_pong(std::uint64_t nonce) { Writer w; w.u64(nonce); return w.data(); }
}
