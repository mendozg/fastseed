#include "fastseed/protocol/serializer.hpp"
#include "fastseed/protocol/protocol_error.hpp"
#include <algorithm>
#include <bit>
#include <functional>

namespace fastseed::protocol {
namespace {
std::size_t compact_width(std::uint64_t n) {
    return n < 253 ? 1 : n <= 0xffff ? 3 : n <= 0xffffffffULL ? 5 : 9;
}
}
void Writer::need(std::size_t n) const {
    if (n > limit_ - data_.size())
        throw ProtocolError(ProtocolErrorCode::MessageTooLarge, "writer size limit");
}
void Writer::integer(std::uint64_t v, unsigned n) {
    need(n);
    for (unsigned i = 0; i < n; ++i) data_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}
void Writer::u8(std::uint8_t v) { integer(v, 1); }
void Writer::u16(std::uint16_t v) { integer(v, 2); }
void Writer::u32(std::uint32_t v) { integer(v, 4); }
void Writer::i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
void Writer::u64(std::uint64_t v) { integer(v, 8); }
void Writer::be16(std::uint16_t v) {
    need(2); u8(static_cast<std::uint8_t>(v >> 8)); u8(static_cast<std::uint8_t>(v));
}
void Writer::compact_size(std::uint64_t v) {
    need(compact_width(v));
    if (v < 253) u8(static_cast<std::uint8_t>(v));
    else if (v <= 0xffff) { u8(253); u16(static_cast<std::uint16_t>(v)); }
    else if (v <= 0xffffffffULL) { u8(254); u32(static_cast<std::uint32_t>(v)); }
    else { u8(255); u64(v); }
}
void Writer::bytes(std::span<const std::uint8_t> v) {
    need(v.size());
    if (v.empty()) return;
    const std::less<const std::uint8_t*> before;
    if (!data_.empty() && !before(v.data(), data_.data()) && before(v.data(), data_.data() + data_.size())) {
        const std::vector<std::uint8_t> copy(v.begin(), v.end());
        data_.insert(data_.end(), copy.begin(), copy.end());
    } else data_.insert(data_.end(), v.begin(), v.end());
}
void Writer::string(const std::string& s) {
    if (s.size() > 0xffff) throw std::length_error("string too long");
    need(compact_width(s.size()) + s.size());
    compact_size(s.size());
    bytes({reinterpret_cast<const std::uint8_t*>(s.data()), s.size()});
}
void Writer::fixed16(std::span<const std::uint8_t> v) {
    if (v.size() != 16) throw std::invalid_argument("fixed16");
    bytes(v);
}
void Reader::need(std::size_t n) const {
    if (n > remaining()) throw ProtocolError(ProtocolErrorCode::Truncated, "truncated input");
}
std::span<const std::uint8_t> Reader::view(std::size_t n) {
    need(n); auto result = data_.subspan(pos_, n); pos_ += n; return result;
}
std::uint8_t Reader::u8() { need(1); return data_[pos_++]; }
std::uint16_t Reader::u16() { auto b = view(2); return std::uint16_t(b[0]) | (std::uint16_t(b[1]) << 8); }
std::uint16_t Reader::be16() { auto b = view(2); return (std::uint16_t(b[0]) << 8) | b[1]; }
std::uint32_t Reader::u32() {
    auto b = view(4); std::uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= std::uint32_t(b[i]) << (8 * i);
    return v;
}
std::int32_t Reader::i32() { return std::bit_cast<std::int32_t>(u32()); }
std::uint64_t Reader::u64() {
    auto b = view(8); std::uint64_t v = 0;
    for (unsigned i = 0; i < 8; ++i) v |= std::uint64_t(b[i]) << (8 * i);
    return v;
}
std::uint64_t Reader::compact_size(std::uint64_t maximum) {
    Reader next = *this;
    const auto marker = next.u8();
    const std::uint64_t value = marker < 253 ? marker : marker == 253 ? next.u16() : marker == 254 ? next.u32() : next.u64();
    if ((marker == 253 && value < 253) || (marker == 254 && value <= 0xffff) || (marker == 255 && value <= 0xffffffffULL))
        throw ProtocolError(ProtocolErrorCode::NonCanonical, "non-canonical CompactSize");
    if (value > maximum) throw ProtocolError(ProtocolErrorCode::MessageTooLarge, "CompactSize exceeds field limit");
    *this = next;
    return value;
}
std::string Reader::varstring(std::size_t maximum) {
    Reader next = *this;
    const auto n = next.compact_size(maximum);
    auto result = next.string(static_cast<std::size_t>(n));
    *this = next;
    return result;
}
std::vector<std::uint8_t> Reader::bytes(std::size_t n) { auto b = view(n); return {b.begin(), b.end()}; }
std::string Reader::string(std::size_t n) {
    auto b = view(n);
    if (b.empty()) return {};
    return {reinterpret_cast<const char*>(b.data()), b.size()};
}
std::array<std::uint8_t,16> Reader::fixed16() {
    auto b = view(16); std::array<std::uint8_t,16> a{}; std::copy(b.begin(), b.end(), a.begin()); return a;
}
void Reader::require_end() const {
    if (remaining()) throw ProtocolError(ProtocolErrorCode::TrailingData, "trailing payload bytes");
}
}
