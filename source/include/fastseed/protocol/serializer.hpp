#pragma once
#include <cstdint>
#include <span>
#include <array>
#include <string>
#include <vector>
#include <limits>
#include "protocol_limits.hpp"
namespace fastseed::protocol {
class Writer {
public:
 explicit Writer(std::size_t limit = limits::max_message_size) : limit_(limit) {}
 void u16(std::uint16_t v); void be16(std::uint16_t v);
 void compact_size(std::uint64_t v);
 void u8(std::uint8_t v); void i32(std::int32_t v); void u32(std::uint32_t v); void u64(std::uint64_t v);
 void bytes(std::span<const std::uint8_t> v); void string(const std::string& s); void fixed16(std::span<const std::uint8_t> v);
 const std::vector<std::uint8_t>& data() const noexcept { return data_; }
private:
 void need(std::size_t n) const;
 void integer(std::uint64_t v, unsigned n);
 std::vector<std::uint8_t> data_;
 std::size_t limit_;
};
class Reader {
public:
 explicit Reader(std::span<const std::uint8_t> data) : data_(data) {}
 std::uint16_t u16(); std::uint16_t be16();
 std::uint64_t compact_size(std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max());
 std::string varstring(std::size_t maximum);
 std::span<const std::uint8_t> view(std::size_t n);
 void require_end() const;
 std::uint8_t u8(); std::int32_t i32(); std::uint32_t u32(); std::uint64_t u64();
 std::vector<std::uint8_t> bytes(std::size_t n); std::string string(std::size_t n); std::array<std::uint8_t,16> fixed16();
 std::size_t remaining() const noexcept { return data_.size()-pos_; }
private: std::span<const std::uint8_t> data_; std::size_t pos_{0}; void need(std::size_t n) const;
};
}
